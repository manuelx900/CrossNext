#include "PocketGamesActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>
#include <esp_random.h>

#include "../Shelf.h"
#include "../ui/MatchSettings.h"
#include "../ui/PixelMenuTheme.h"
#include "components/SideButtonNavigation.h"

namespace pg = pocketgames;
namespace {
constexpr uint32_t kOthelloObserveMs = 1000;
constexpr uint32_t kOthelloPlacementMs = 1000;
}  // namespace

std::unique_ptr<Activity> PocketGamesActivity::create(pg::Kind kind, GfxRenderer& renderer, MappedInputManager& input) {
  // Retained UI/store work exceeds the small stack budget; one checked owner, released on exit.
  auto result = makeUniqueNoThrow<PocketGamesActivity>(kind, renderer, input);
  if (!result) LOG_ERR("POCKET-GAMES", "OOM: activity (%u bytes)", static_cast<unsigned>(sizeof(PocketGamesActivity)));
  return result;
}
std::unique_ptr<Activity> PocketGamesActivity::createOthello(GfxRenderer& r, MappedInputManager& i) {
  return create(pg::Kind::Othello, r, i);
}
void PocketGamesActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  saveFailed = !store.load(kind, state);
  if (!store.hasSave()) {
    pg::start(state, kind, esp_random());
    dirty = true;
  }
  level = defaultLevel = state.level;
  if (kind == pg::Kind::Othello) {
    uint8_t bytes[3]{};
    HalFile file;
    if (Storage.openFileForRead("OTHELLO", "/.crosspoint/othello.cfg", file) && file.read(bytes, 3) == 3 &&
        bytes[0] == 1 && bytes[1] < 3 && bytes[2] >= 1 && bytes[2] <= 2) {
      level = defaultLevel = bytes[1];
      preferredColour = savedColour = bytes[2];
      savedLevel = defaultLevel;
    } else
      level = defaultLevel = 1;
    humanColour = state.humanColour;
  }
  lastSave = lastTick = millis();
  requestUpdate();
}
void PocketGamesActivity::saveOthelloPreferences() {
  if (savedColour == preferredColour && savedLevel == defaultLevel) return;
  const uint8_t bytes[] = {1, static_cast<uint8_t>(defaultLevel), static_cast<uint8_t>(preferredColour)};
  HalFile file;
  if (!Storage.openFileForWrite("OTHELLO", "/.crosspoint/othello.cfg", file) ||
      file.write(bytes, sizeof(bytes)) != sizeof(bytes)) {
    LOG_ERR("OTHELLO", "Could not save preferences");
    saveFailed = true;
  } else {
    savedColour = preferredColour;
    savedLevel = defaultLevel;
  }
}
void PocketGamesActivity::save() {
  if (!dirty) return;
  saveFailed = !store.save(state);
  if (!saveFailed) dirty = false;
  lastSave = millis();
}
void PocketGamesActivity::prepareForSleep() {
  cancelBotMove();
  save();
}
void PocketGamesActivity::onExit() {
  save();
  Activity::onExit();
}
void PocketGamesActivity::changed() {
  ++revision;
  ready = false;
  othelloInput = BoardInput::AwaitFreshFrame;
  requestUpdate();
}
void PocketGamesActivity::cancelBotMove() {
  botMovePhase = BotMovePhase::Idle;
  botSquare = -1;
}
uint32_t PocketGamesActivity::surfaceMeaning() const {
  return paintclock::mixMeaning(paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(view)),
                                revision);
}
void PocketGamesActivity::action(freeink::ui::ActionId action, int value) {
  switch (action) {
    case pocketui::Settings:
      level = defaultLevel;
      view = pocketui::View::Level;
      break;
    case matchsettings::Difficulty:
      if (kind != pg::Kind::Othello || value < 0 || value > 2) return;
      level = value;
      break;
    case matchsettings::Colour:
      if (kind != pg::Kind::Othello) return;
      preferredColour = value ? 1 : 2;
      break;
    case matchsettings::Done:
      if (kind != pg::Kind::Othello) return;
      defaultLevel = level;
      state.level = level;
      dirty = true;
      save();
      saveOthelloPreferences();
      view = pocketui::View::Menu;
      break;
    case pocketui::Play:
      view = kind == pg::Kind::Othello && pg::finished(state) ? pocketui::View::Result : pocketui::View::Board;
      lastTick = millis();
      break;
    case pocketui::New:
      restart = false;
      view = pocketui::View::ConfirmNew;
      break;
    case pocketui::Help:
      guidePage = 0;
      view = pocketui::View::Help;
      break;
    case pocketui::GuidePage:
      if (kind != pg::Kind::Othello || view != pocketui::View::Help || value < 0 || value > 2 || value == guidePage)
        return;
      guidePage = value;
      break;
    case pocketui::ViewBoard:
      if (kind != pg::Kind::Othello || !pg::finished(state)) return;
      view = pocketui::View::Review;
      break;
    case pocketui::ViewResult:
      if (kind != pg::Kind::Othello || !pg::finished(state)) return;
      view = pocketui::View::Result;
      break;
    case pocketui::ConfirmNew:
      pg::start(state, kind, restart ? state.seed : esp_random(), restart ? state.level : defaultLevel);
      if (kind == pg::Kind::Othello) {
        if (!restart) humanColour = preferredColour;
        state.humanColour = humanColour;
      }
      selected = -1;
      view = pocketui::View::Board;
      dirty = true;
      save();
      break;
    case pocketui::Restart:
      restart = true;
      view = pocketui::View::ConfirmNew;
      level = defaultLevel;
      break;
    case pocketui::Cancel:
      view = pocketui::View::Menu;
      level = defaultLevel;
      break;
    case pocketui::Menu:
      view = pocketui::View::Menu;
      save();
      break;
    default:
      return;
  }
  cancelBotMove();
  invalid = false;
  changed();
}
void PocketGamesActivity::loop() {
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    cancelBotMove();
    if (view == pocketui::View::Menu) {
      save();
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (kind == pg::Kind::Othello && view == pocketui::View::Level) {
      defaultLevel = level;
      state.level = level;
      dirty = true;
      saveOthelloPreferences();
    }
    view = view == pocketui::View::Review ? pocketui::View::Result : pocketui::View::Menu;
    level = defaultLevel;
    save();
    changed();
    return;
  }
  if (kind == pg::Kind::Othello && view == pocketui::View::Help && ready && surfaceRevealed()) {
    if (const int direction = sidebuttons::swipeStep(mappedInput)) {
      action(pocketui::GuidePage, guidePage + direction);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::PageBack)) {
      action(pocketui::GuidePage, guidePage - 1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::PageForward)) {
      action(pocketui::GuidePage, guidePage + 1);
      return;
    }
  }
  int x = 0, y = 0;
  const bool tapped = ready && sidebuttons::poll(interactions, mappedInput, x, y,
                                                 !(kind == pg::Kind::Othello && view == pocketui::View::Level));
  if (tapped) {
    freeink::ui::InputSnapshot tap;
    tap.touchReleased = true;
    tap.touchX = x;
    tap.touchY = y;
    const auto event = sidebuttons::route(interactions, tap, mappedInput, *this,
                                          !(kind == pg::Kind::Othello && view == pocketui::View::Level));
    if (event.action) {
      action(event.action, event.value);
      return;
    }
  }
  if (dirty && millis() - lastSave >= 3000u) save();
  if (view == pocketui::View::Board && ready && surfaceRevealed()) {
    if (kind == pg::Kind::Othello && (state.othello.over || state.othello.turn != humanColour)) {
      switch (botMovePhase) {
        case BotMovePhase::AwaitPaint:
          return;
        case BotMovePhase::Showing: {
          if (static_cast<uint32_t>(millis() - botPreviewAt) < kOthelloPlacementMs) return;
          const int square = botSquare;
          cancelBotMove();
          if (!pg::play(state.othello, square)) {
            LOG_ERR("OTHELLO", "Could not apply computer move at %d", square);
            invalid = true;
            changed();
            return;
          }
          dirty = true;
          if (state.othello.over) save();
          changed();
          return;
        }
        case BotMovePhase::Idle:
          break;
      }
      if (static_cast<uint32_t>(millis() - lastTick) < kOthelloObserveMs) return;
      if (state.othello.over) {
        view = pocketui::View::Result;
        changed();
        return;
      }
      botSquare = pg::chooseBotMove(state.othello, defaultLevel);
      if (botSquare < 0) {
        LOG_ERR("OTHELLO", "No legal computer move on an unfinished board");
        view = pocketui::View::Menu;
      } else
        botMovePhase = BotMovePhase::AwaitPaint;
      changed();
      return;
    }
    if (kind == pg::Kind::Othello) {
      switch (othelloInput) {
        case BoardInput::AwaitFreshFrame:
          // Input was sampled before taking RenderLock and may predate this paint.
          othelloInput = BoardInput::AwaitIdle;
          return;
        case BoardInput::AwaitIdle:
          if (!mappedInput.isScreenTouchHeld(x, y) && !mappedInput.wasScreenTouchReleased())
            othelloInput = BoardInput::Armed;
          return;
        case BoardInput::Armed:
          break;
      }
    }
  }
  if (!tapped || !sidebuttons::hasActivation(x, y)) return;
  if (view != pocketui::View::Board || !surfaceRevealed() || pg::finished(state)) return;
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const auto device = target.deviceContext();
  bool edited = false;
  invalid = false;
  switch (kind) {
    case pg::Kind::Othello:
      if (state.othello.turn != humanColour) return;
      edited = pg::play(state.othello, pocketui::squareAt(device, 8, x, y));
      invalid = !edited;
      break;
  }
  dirty |= edited;
  if (edited && pg::finished(state)) save();
  changed();
}
void PocketGamesActivity::render(RenderLock&&) {
  if (kind == pg::Kind::Othello && view == pocketui::View::Level) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const freeink::ui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  pocketui::build(
      screen, {&state, view, saveFailed, invalid, selected, level, guidePage, humanColour, preferredColour, botSquare});
  toybox::reportOverflow(interactions, "PocketGames", &renderer);
  ready = true;
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  // Start the observation interval after the panel shows the completed move.
  if (kind == pg::Kind::Othello && view == pocketui::View::Board) {
    lastTick = millis();
    if (botMovePhase == BotMovePhase::AwaitPaint) {
      botPreviewAt = lastTick;
      botMovePhase = BotMovePhase::Showing;
    }
  }
}
