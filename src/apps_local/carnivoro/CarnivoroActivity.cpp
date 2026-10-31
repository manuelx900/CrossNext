#include "CarnivoroActivity.h"

#include <FontCacheManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ToyboxFonts.h"
#include "CrossPointSettings.h"
#include "components/SideButtonNavigation.h"
static_assert(sizeof(CarnivoroActivity) <= 16 * 1024, "Carnivoro keeps a fixed activity workspace");
namespace fui = freeink::ui;
namespace {
constexpr char live[] = "/.crosspoint/carnivoro.bin", backup[] = "/.crosspoint/carnivoro.bak",
               temp[] = "/.crosspoint/carnivoro.tmp";
}
std::unique_ptr<Activity> CarnivoroActivity::create(GfxRenderer& r, MappedInputManager& i) {
  // Activity-owned progress, staged edit, verification and wire storage are about 12 KiB;
  // too large for the task stack, reused until exit without per-frame allocation.
  auto result = makeUniqueNoThrow<CarnivoroActivity>(r, i);
  if (!result) LOG_ERR("CARN", "OOM: activity %u bytes", unsigned(sizeof(CarnivoroActivity)));
  return result;
}
void CarnivoroActivity::onEnter() {
  Activity::onEnter();
  load();
  model.state = &state;
  resetUi();
  app.setScreen(&CarnivoroActivity::screen, this);
  requestUpdate();
}
void CarnivoroActivity::onExit() {
  flush();
  Activity::onExit();
}
void CarnivoroActivity::prepareForSleep() { flush(); }
CarnivoroActivity::ReadResult CarnivoroActivity::read(const char* name, carnivoro::State& target) {
  HalFile file;
  if (!Storage.openFileForRead("CARN", name, file)) return ReadResult::IoError;
  const auto size = file.size();
  if (size > sizeof(wire)) {
    if (file.read(wire, 5) != 5) return ReadResult::IoError;
    return !std::memcmp(wire, "CARN", 4) && wire[4] > carnivoro::kSaveVersion ? ReadResult::Future
                                                                              : ReadResult::Invalid;
  }
  if (file.read(wire, size) != int(size)) return ReadResult::IoError;
  bool future = false;
  if (carnivoro::decode(wire, size, target, future)) return ReadResult::Valid;
  return future ? ReadResult::Future : ReadResult::Invalid;
}
void CarnivoroActivity::load() {
  if (!Storage.ready()) {
    writable = false;
    model.saveError = true;
    return;
  }
  const auto current = read(live, state);
  if (current == ReadResult::Valid) return;
  if (current == ReadResult::Future) {
    writable = false;
    model.saveError = true;
    return;
  }
  const auto recovery = read(backup, state);
  if (recovery == ReadResult::Valid) return;
  if (recovery == ReadResult::Future || Storage.exists(live) || Storage.exists(backup)) {
    writable = false;
    model.saveError = true;
    LOG_ERR("CARN", "Unreadable or newer save; preserving attempts");
  }
}
bool CarnivoroActivity::save(const carnivoro::State& value) {
  if (!writable || !Storage.ready() || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  if (Storage.exists(backup)) {
    const auto recovery = read(backup, verified);
    if (recovery == ReadResult::Future) {
      writable = false;
      return false;
    }
    if (recovery == ReadResult::IoError) return false;
  }
  const size_t size = carnivoro::encode(value, wire, sizeof(wire));
  if (!size) return false;
  {
    HalFile file;
    if (!Storage.openFileForWrite("CARN", temp, file) || file.write(wire, size) != size) return false;
    file.flush();
  }
  if (read(temp, verified) != ReadResult::Valid || std::memcmp(&verified, &value, sizeof(value))) return false;
  if (Storage.exists(live)) {
    const auto current = read(live, verified);
    if (current == ReadResult::Valid) {
      if (Storage.exists(backup) && !Storage.remove(backup)) return false;
      if (!Storage.rename(live, backup)) return false;
    } else {
      if (current == ReadResult::Future) {
        writable = false;
        return false;
      }
      if (current == ReadResult::IoError) return false;
      const auto recovery = read(backup, verified);
      if (recovery == ReadResult::Future) {
        writable = false;
        return false;
      }
      if (recovery != ReadResult::Valid) return false;
      if (!Storage.remove(live)) return false;
    }
  }
  if (!Storage.rename(temp, live)) return false;
  if (Storage.exists(backup)) Storage.remove(backup);
  return true;
}
bool CarnivoroActivity::flush() {
  if (!dirty && pendingOperation == PendingOperation::None) return !model.saveError;
  if (!save(pendingOperation != PendingOperation::None ? pending : state)) {
    model.saveError = true;
    LOG_ERR("CARN", "SD commit failed");
    return false;
  }
  if (pendingOperation != PendingOperation::None) {
    state = pending;
    switch (pendingOperation) {
      case PendingOperation::Accusation:
        model.view = carnivoroui::View::Verdict;
        break;
      case PendingOperation::Replay:
        model.view = carnivoroui::View::Intro;
        break;
      case PendingOperation::Reset:
        model.view = carnivoroui::View::Menu;
        model.casePage = 0;
        break;
      case PendingOperation::None:
        break;
    }
    pendingOperation = PendingOperation::None;
    model.page = 0;
  }
  dirty = false;
  model.saveError = false;
  return true;
}
void CarnivoroActivity::changed() {
  dirty = true;
  changedAt = millis();
  resetUi();
  requestUpdate();
}
void CarnivoroActivity::openCase(int id) {
  if (!carnivoro::getCase(id) || !carnivoro::available(state, id, model.campaign)) return;
  if (state.caseId != id) {
    state.caseId = id;
    state.picks = 0;
    state.innocents = 0;
    state.replaying = false;
    state.replayStatus = carnivoro::Status::New;
    state.replayMisses = 0;
    for (auto& n : state.notes) n = carnivoro::Note::Unknown;
  }
  if (state.status[id] == carnivoro::Status::New) state.status[id] = carnivoro::Status::Active;
  state.difficulty = carnivoro::difficulty(id);
  model.casePage = carnivoro::caseIndex(id) / geometry.caseCapacity;
  model.solution = carnivoro::solve(*carnivoro::getCase(id));
  model.view = carnivoro::closed(state, id) ? carnivoroui::View::Verdict : carnivoroui::View::Intro;
  model.page = 0;
  model.openSuspect = -1;
  changed();
}
void CarnivoroActivity::back() {
  using carnivoroui::View;
  if (model.saveError) {
    if (!writable || !flush()) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    resetUi();
    requestUpdate();
    return;
  }
  switch (model.view) {
    case View::Menu:
      shelf::leave(renderer, mappedInput);
      return;
    case View::Cases:
    case View::Help:
    case View::ResetConfirm:
      model.view = View::Menu;
      break;
    case View::Intro:
      model.view = View::Menu;
      break;
    case View::Overview:
      model.view = carnivoro::closed(state, state.caseId) ? View::Solution : View::Statements;
      model.focus = 0;
      resetUi();
      requestUpdate();
      return;
    case View::Statements:
      model.view = View::Intro;
      break;
    case View::Accuse:
      model.view = View::Statements;
      break;
    case View::Solution:
      model.view = View::Verdict;
      break;
    case View::Verdict:
      model.view = carnivoro::closed(state, state.caseId) ? View::Cases : View::Statements;
      break;
  }
  model.page = 0;
  model.focus = 0;
  resetUi();
  requestUpdate();
}
void CarnivoroActivity::dispatch(int action, int value) {
  using namespace carnivoroui;
  if (model.saveError && action != RetrySave && action != Back) return;
  const auto* c = carnivoro::getCase(state.caseId);
  switch (action) {
    case Open: {
      model.campaign = true;
      const int id = carnivoro::nextCase(state);
      if (id >= 0)
        openCase(id);
      else {
        model.view = View::Cases;
        model.casePage = (carnivoro::caseCount(state.difficulty) - 1) / geometry.caseCapacity;
      }
      break;
    }
    case Campaign:
      model.campaign = true;
      model.casePage = state.caseId < carnivoro::kCases && carnivoro::difficulty(state.caseId) == state.difficulty
                           ? carnivoro::caseIndex(state.caseId) / geometry.caseCapacity
                           : 0;
      model.view = View::Cases;
      break;
    case SelectDifficulty:
      if (model.view != View::Cases || value < 0 || value > static_cast<int>(carnivoro::Difficulty::Expert) ||
          value == static_cast<int>(state.difficulty))
        return;
      state.difficulty = static_cast<carnivoro::Difficulty>(value);
      model.casePage = 0;
      changed();
      break;
    case Help:
      model.view = View::Help;
      model.page = 0;
      break;
    case ResetAll:
      model.view = View::ResetConfirm;
      break;
    case ConfirmReset:
      if (model.view != View::ResetConfirm) return;
      carnivoro::resetProgress(pending);
      pendingOperation = PendingOperation::Reset;
      flush();
      break;
    case Replay:
      if (!c || !carnivoro::closed(state, state.caseId)) return;
      pending = state;
      if (!carnivoro::restartCase(pending, state.caseId)) return;
      pendingOperation = PendingOperation::Replay;
      flush();
      break;
    case ChooseCase:
      openCase(value);
      break;
    case Start:
      if (!c) return;
      model.view = carnivoro::closed(state, state.caseId) ? View::Verdict : View::Statements;
      model.page = 0;
      break;
    case Previous:
    case Next: {
      const int delta = action == Next ? 1 : -1;
      const int oldPage = model.page, oldCasePage = model.casePage;
      if (model.view == View::Cases)
        model.casePage =
            std::clamp(model.casePage + delta, 0, (carnivoro::caseCount(state.difficulty) - 1) / geometry.caseCapacity);
      else if (model.view == View::Help)
        model.page = std::clamp(model.page + delta, 0, std::max(0, model.textPages - 1));
      else if (c && model.view != View::Overview)
        model.page = std::clamp(model.page + delta, 0, (c->count - 1) / geometry.suspectCapacity);
      if (oldPage == model.page && oldCasePage == model.casePage) return;
      break;
    }
    case Accuse:
      if (!c) return;
      model.view = carnivoro::closed(state, state.caseId) ? View::Verdict : View::Accuse;
      model.page = 0;
      break;
    case Note:
      if (!c || value < 0 || value >= c->count * 3 || carnivoro::closed(state, state.caseId)) return;
      state.notes[value / 3] = static_cast<carnivoro::Note>(value % 3);
      changed();
      break;
    case Mark:
      if (!c || value < 0 || value >= c->count * 3 ||
          !carnivoro::setGuiltNote(state, value / 3, static_cast<carnivoro::GuiltNote>(value % 3)))
        return;
      changed();
      break;
    case CycleNote:
      if (!c || value < 0 || value >= c->count || carnivoro::closed(state, state.caseId)) return;
      state.notes[value] = static_cast<carnivoro::Note>((static_cast<int>(state.notes[value]) + 1) % 3);
      changed();
      break;
    case CycleGuilt:
      if (!c || value < 0 || value >= c->count ||
          !carnivoro::setGuiltNote(
              state, value,
              static_cast<carnivoro::GuiltNote>((static_cast<int>(carnivoro::guiltNote(state, value)) + 1) % 3)))
        return;
      changed();
      break;
    case ShowOverview:
      if (!c) return;
      model.view = View::Overview;
      break;
    case ReadStatement:
      if (!c || value < 0 || value >= c->count) return;
      model.view = carnivoro::closed(state, state.caseId) ? View::Solution : View::Statements;
      model.openSuspect = value;
      break;
    case Pick:
      if (!c || value < 0 || value >= c->count) return;
      if (carnivoro::closed(state, state.caseId)) return;
      if (c->culprits == 1)
        state.picks = state.picks == (1 << value) ? 0 : (1 << value);
      else
        state.picks ^= 1 << value;
      state.innocents &= ~state.picks;
      changed();
      break;
    case Confirm:
      if (!c || model.view != View::Accuse) return;
      pending = state;
      if (!carnivoro::accuse(pending, state.caseId, state.picks)) {
        LOG_ERR("CARN", "Rejected accusation status %d remaining %d worlds %d", int(state.status[state.caseId]),
                carnivoro::remaining(state, state.caseId), carnivoro::solve(*c).worlds);
        return;
      }
      pendingOperation = PendingOperation::Accusation;
      flush();
      break;
    case Continue:
      if (!c) return;
      if (!carnivoro::closed(state, state.caseId)) {
        model.view = View::Statements;
        model.page = 0;
      } else {
        const int id = carnivoro::nextCase(state);
        if (id >= 0)
          openCase(id);
        else {
          model.view = View::Cases;
          model.casePage = (carnivoro::caseCount(state.difficulty) - 1) / geometry.caseCapacity;
        }
      }
      break;
    case ViewSolution:
      if (!c || !carnivoro::closed(state, state.caseId)) return;
      model.view = View::Solution;
      model.page = 0;
      break;
    case Menu:
      model.view = View::Menu;
      break;
    case RetrySave:
      if (!dirty && pendingOperation == PendingOperation::None) {
        writable = true;
        model.saveError = false;
        load();
      } else
        flush();
      break;
    case Back:
      back();
      return;
    default:
      return;
  }
  model.focus = 0;
  resetUi();
  requestUpdate();
}
void CarnivoroActivity::screen(UiScreen& screen, void* p) {
  auto& self = *static_cast<CarnivoroActivity*>(p);
  pixelui::begin(screen);
  pixelui::bindFonts(self.uiTarget, self.renderer, true);
  self.uiTarget.setFont(3, toybox::kTileFontId);
  self.uiTarget.setFont(4, toybox::kLibraryFontId);
  carnivoroui::build(screen, self.model, self.geometry, self.wrap);
}
void CarnivoroActivity::render(RenderLock&&) {
  renderer.clearScreen();
  pixelui::drawHeader(renderer, tr(STR_CAR_TITLE), true);
  renderUi();
  renderer.displayBuffer();
}
void CarnivoroActivity::loop() {
  RenderLock lock;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    back();
    return;
  }
  if (dirty && pendingOperation == PendingOperation::None && millis() - changedAt > 3000) {
    flush();
    if (model.saveError) {
      resetUi();
      requestUpdate();
      return;
    }
  }
  if (routingReady()) {
    if (const int direction = sidebuttons::horizontalStep(mappedInput)) {
      const auto pageAction = direction > 0 ? carnivoroui::Next : carnivoroui::Previous;
      for (int n = 0; n < geometry.count; ++n) {
        if (geometry.hits[n].action != pageAction) continue;
        dispatch(pageAction);
        return;
      }
    }
    if (const int direction = sidebuttons::longStep(mappedInput)) {
      if (model.view == carnivoroui::View::Cases || model.view == carnivoroui::View::Help ||
          model.view == carnivoroui::View::Statements || model.view == carnivoroui::View::Solution) {
        dispatch(direction > 0 ? carnivoroui::Next : carnivoroui::Previous);
        return;
      }
    }
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      if (model.view == carnivoroui::View::Cases || model.view == carnivoroui::View::Help ||
          model.view == carnivoroui::View::Statements || model.view == carnivoroui::View::Solution ||
          model.view == carnivoroui::View::Overview || model.view == carnivoroui::View::Accuse ||
          model.view == carnivoroui::View::Verdict) {
        dispatch(direction > 0 ? carnivoroui::Next : carnivoroui::Previous);
        return;
      }
    }
  }
  const auto action = routeTouch(mappedInput, false, false, model.view != carnivoroui::View::Cases);
  if (action.routed && app.invalidated()) requestUpdate();

  if (action) {
    app.clearTapFlash();
    dispatch(action.event.action, action.event.value);
    return;
  }
  if (action.routed) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left))
    dispatch(carnivoroui::Previous);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right))
    dispatch(carnivoroui::Next);
  else if (model.view == carnivoroui::View::Cases)
    return;
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
           mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    model.focus = std::clamp(model.focus + (mappedInput.wasReleased(MappedInputManager::Button::Up) ? -1 : 1), 0,
                             std::max(0, geometry.count - 1));
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm) && geometry.count) {
    const auto hit = geometry.hits[std::clamp(model.focus, 0, geometry.count - 1)];
    dispatch(hit.action, hit.value);
  }
}
