#include "ChessPuzzleActivity.h"

#include <CrossPointSettings.h>
#include <I18n.h>
#include <Memory.h>

#include <cstdio>
#include <cstdlib>

#include "../../../activities/util/KeyboardEntryActivity.h"
#include "../../ui/PixelMenuTheme.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;
std::unique_ptr<Activity> ChessPuzzleActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  auto activity = makeUniqueNoThrow<ChessPuzzleActivity>(renderer, input);
  if (!activity)
    LOG_ERR("CHESS-PUZZLE", "OOM: puzzle activity (%u bytes)", static_cast<unsigned>(sizeof(ChessPuzzleActivity)));
  return activity;
}
void ChessPuzzleActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  pieceDesign = chessstyle::loadPreference();
  saveFailed = !store.load(progress);
  lastSave = millis();
  if (progress.lastIndex < chesspuzzle::kCount) {
    index = progress.lastIndex;
    group = index < 400 ? 0 : index < 750 ? 1 : 2;
    page = progress.cursor[group] / 16;
  }
  requestUpdate();
}
void ChessPuzzleActivity::save() {
  if (!dirty) return;
  saveFailed = !store.save(progress);
  if (!saveFailed) dirty = false;
  lastSave = millis();
}
void ChessPuzzleActivity::onExit() {
  save();
  Activity::onExit();
}
void ChessPuzzleActivity::prepareForSleep() { save(); }
void ChessPuzzleActivity::open(int next) {
  const int count = chesspuzzle::kGroupCounts[group];
  const int row = (next - chesspuzzle::kGroupStarts[group] + count) % count;
  index = chesspuzzle::kGroupStarts[group] + row;
  if (progress.cursor[group] != row) {
    progress.cursor[group] = static_cast<uint16_t>(row);
    dirty = true;
  }
  if (progress.lastIndex != index) {
    progress.lastIndex = static_cast<uint16_t>(index);
    dirty = true;
  }
  invalidNumber = false;
  chesspuzzle::load(session, index);
  view = chesspuzzleui::View::Board;
  ready = false;
  requestUpdate();
}
void ChessPuzzleActivity::choose() {
  page = std::clamp((index - chesspuzzle::kGroupStarts[group]) / 16, 0, (chesspuzzle::kGroupCounts[group] - 1) / 16);
  view = chesspuzzleui::View::Select;
  ready = false;
  requestUpdate();
}
void ChessPuzzleActivity::completeMove() {
  if (session.status == chesspuzzle::Status::Solved) {
    if (chesspuzzle::markSolved(progress, index)) {
      dirty = true;
      save();
    }
    view = chesspuzzleui::View::Celebration;
  }
  ready = false;
  requestUpdate();
}
void ChessPuzzleActivity::action(fui::ActionId action, int value) {
  switch (action) {
    case chesspuzzleui::Difficulty:
      if (value < 0 || value > 2) return;
      group = value;
      page = progress.cursor[group] / 16;
      view = chesspuzzleui::View::Select;
      ready = false;
      requestUpdate();
      return;
    case chesspuzzleui::Page: {
      const int next = std::clamp(page + value, 0, (chesspuzzle::kGroupCounts[group] - 1) / 16);
      if (next == page) return;
      page = next;
    }
      ready = false;
      requestUpdate();
      return;
    case chesspuzzleui::Pick:
      if (value < chesspuzzle::kGroupStarts[group] ||
          value >= chesspuzzle::kGroupStarts[group] + chesspuzzle::kGroupCounts[group])
        return;
      open(value);
      return;
    case chesspuzzleui::Resume:
      if (progress.lastIndex < chesspuzzle::kCount) {
        group = progress.lastIndex < 400 ? 0 : progress.lastIndex < 750 ? 1 : 2;
        if (session.source == chesspuzzle::puzzle(progress.lastIndex) && !chesspuzzle::terminal(session)) {
          index = progress.lastIndex;
          view = chesspuzzleui::View::Board;
          ready = false;
          requestUpdate();
        } else
          open(progress.lastIndex);
      }
      return;
    case chesspuzzleui::Choose:
      choose();
      return;
    case chesspuzzleui::Reset:
      view = chesspuzzleui::View::ConfirmReset;
      break;
    case chesspuzzleui::ConfirmReset:
      saveFailed = !store.reset(progress);
      if (!saveFailed) {
        dirty = false;
        index = group = 0;
      }
      view = chesspuzzleui::View::Difficulty;
      break;
    case chesspuzzleui::Cancel:
      view = session.source && progress.lastIndex < chesspuzzle::kCount ? chesspuzzleui::View::Board
                                                                        : chesspuzzleui::View::Difficulty;
      break;
    case chesspuzzleui::Review:
      view = chesspuzzleui::View::Board;
      break;
    case chesspuzzleui::Promote:
      if (value != chess::Queen && value != chess::Rook && value != chess::Bishop && value != chess::Knight) return;
      chesspuzzle::play(session, promotionFrom, promotionTo, value);
      view = chesspuzzleui::View::Board;
      completeMove();
      return;
    case chesspuzzleui::Restart:
      open(index);
      return;
    case chesspuzzleui::Previous:
      open(index - 1);
      return;
    case chesspuzzleui::Next:
      open(index + 1);
      return;
    default:
      return;
  }
  ready = false;
  requestUpdate();
}
uint32_t ChessPuzzleActivity::surfaceMeaning() const {
  auto meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(view));
  meaning = paintclock::mixMeaning(meaning, static_cast<uint32_t>(index));
  meaning = paintclock::mixMeaning(meaning, static_cast<uint32_t>(session.ply));
  return paintclock::mixMeaning(meaning, static_cast<uint32_t>(session.status));
}
void ChessPuzzleActivity::loop() {
  RenderLock lock(*this);
  if (dirty && millis() - lastSave >= 1500) save();
  if (view == chesspuzzleui::View::Board && session.status == chesspuzzle::Status::Thinking) {
    if (ready && surfaceRevealed()) {
      chesspuzzle::reply(session, searchBuffers);
      completeMove();
    }
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    save();
    if (view == chesspuzzleui::View::Difficulty) {
      finish();
      return;
    }
    switch (view) {
      case chesspuzzleui::View::Select:
      case chesspuzzleui::View::ConfirmReset:
        view = chesspuzzleui::View::Difficulty;
        break;
      case chesspuzzleui::View::Board:
      case chesspuzzleui::View::Celebration:
      case chesspuzzleui::View::Promotion:
        view = chesspuzzleui::View::Select;
        break;
      case chesspuzzleui::View::Difficulty:
        break;
    }
    page = std::max(0, (index - chesspuzzle::kGroupStarts[group]) / 16);
    ready = false;
    requestUpdate();
    return;
  }
  if (view == chesspuzzleui::View::Board) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      open(index + 1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      open(index - 1);
      return;
    }
  }
  if (view == chesspuzzleui::View::Select && ready && interactions.routable()) {
    const auto event = sidebuttons::pageSwipe(interactions, mappedInput);
    if (event.action) {
      action(event.action, event.value);
      return;
    }
    const int held = sidebuttons::longStep(mappedInput);
    if (const int direction = held ? held : sidebuttons::shortStep(mappedInput)) {
      action(chesspuzzleui::Page, direction);
      return;
    }
  }
  int x, y;
  if (!ready || !sidebuttons::poll(interactions, mappedInput, x, y, view == chesspuzzleui::View::Board)) return;
  fui::InputSnapshot tap;
  tap.touchReleased = true;
  tap.touchX = x;
  tap.touchY = y;
  const auto event = sidebuttons::route(interactions, tap, mappedInput, *this, view == chesspuzzleui::View::Board);
  if (event.action) {
    action(event.action, event.value);
    return;
  }
  if (view != chesspuzzleui::View::Board || !session.source || !surfaceRevealed() || chesspuzzle::terminal(session))
    return;
  auto target = toybox::makeTarget(renderer);
  const int square = chesspuzzleui::squareAt(target.deviceContext(), session.source->white, x, y);
  if (square < 0) return;
  if (chess::isColour(session.position.squares[square], session.position.whiteToMove)) {
    session.selected = session.selected == square ? -1 : square;
  } else if (session.selected >= 0) {
    for (int n = 0; n < session.legal.count; ++n) {
      const auto& move = session.legal.moves[n];
      if (move.from == session.selected && move.to == square && move.promotion) {
        promotionFrom = session.selected;
        promotionTo = square;
        view = chesspuzzleui::View::Promotion;
        ready = false;
        requestUpdate();
        return;
      }
    }
    chesspuzzle::play(session, session.selected, square);
    completeMove();
  } else
    return;
  requestUpdate();
}
void ChessPuzzleActivity::render(RenderLock&&) {
  if (view != chesspuzzleui::View::Board) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  chesspuzzleui::build(screen, {&session, &progress, view, group, index, page, saveFailed, invalidNumber, pieceDesign},
                       work);
  ready = true;
  if (view == chesspuzzleui::View::Select) interactions.limitButtonActions({chesspuzzleui::Pick});
  toybox::reportOverflow(interactions, "ChessPuzzles", &renderer);
  renderer.displayBuffer();
}
