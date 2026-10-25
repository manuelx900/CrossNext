#include "WordleActivity.h"

#include <I18n.h>
#include <Memory.h>
#include <esp_random.h>

#include <cstdio>
#include <cstring>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "components/SideButtonNavigation.h"
std::unique_ptr<Activity> WordleActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  // One bounded activity workspace owns the board, two-slot save buffer and controls.
  auto activity = makeUniqueNoThrow<WordleActivity>(renderer, input);
  if (!activity) LOG_ERR("WORDLE", "OOM: activity");
  return activity;
}
void WordleActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  saveFailed = !store.load(state);
  state.language = I18N.getLanguage() == Language::ES ? 1 : 0;
  if (state.seed == 1 && !state.active && !state.record.played) state.seed = esp_random();
  lastSave = millis();
  requestUpdate();
}
void WordleActivity::save() {
  if (!dirty) return;
  saveFailed = !store.save(state);
  if (!saveFailed) dirty = false;
  lastSave = millis();
}
void WordleActivity::prepareForSleep() { save(); }
void WordleActivity::onExit() {
  save();
  Activity::onExit();
}
void WordleActivity::play(bool replace) {
  if (state.active && !state.game.over() &&
      (state.game.letters() != state.letters || state.gameLanguage != state.language) && !replace)
    view = View::ConfirmNew;
  else {
    if (replace || !state.active || state.game.over()) {
      localwordle::record(state, replace);
      localwordle::start(state);
      dirty = true;
      save();
    }
    view = View::Game;
    message = nullptr;
  }
  ready = false;
  requestUpdate();
}
void WordleActivity::submit() {
  const auto result = state.game.submit(
      [](void* context, const char* word) {
        const auto& game = static_cast<WordleActivity*>(context)->state.game;
        return localwordle::contains(game.letters(), word, static_cast<WordleActivity*>(context)->state.gameLanguage) ||
               !std::memcmp(word, game.answer(), game.letters());
      },
      this);
  message = nullptr;
  switch (result) {
    case wordle::Game::Submit::Short:
      message = tr(STR_WORDLE_SHORT);
      return;
    case wordle::Game::Submit::NotAWord:
      message = tr(STR_WORDLE_UNKNOWN);
      return;
    case wordle::Game::Submit::Over:
      return;
    case wordle::Game::Submit::Won:
    case wordle::Game::Submit::Lost:
      localwordle::record(state);
      break;
    case wordle::Game::Submit::Scored:
      break;
  }
  dirty = true;
  save();
}
void WordleActivity::loop() {
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    save();
    if (view == View::Menu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    view = View::Menu;
    ready = false;
    requestUpdate();
    return;
  }
  int x = 0, y = 0;
  if (ready && sidebuttons::poll(interactions, mappedInput, x, y,
                                 view != View::Game && view != View::HowTo && view != View::Lengths)) {
    freeink::ui::InputSnapshot tap;
    tap.touchReleased = true;
    tap.touchX = x;
    tap.touchY = y;
    const auto event = sidebuttons::route(interactions, tap, mappedInput, *this,
                                          view != View::Game && view != View::HowTo && view != View::Lengths);
    if (event.action == wordleui::ActionKeyboard && view == View::Game) {
      const auto key = wordleui::keyAt(keys, x, y);
      if (key != wordleui::kEnter) message = nullptr;
      if (key == wordleui::kEnter)
        submit();
      else if (key == wordleui::kErase)
        dirty |= state.game.erase();
      else if (key >= 'A' && key <= '[')
        dirty |= state.game.type(key);
    } else if (event.action == wordleui::ActionMenu) {
      if (event.value == 0) {
        play();
        return;
      }
      if (event.value == 1) view = View::HowTo;
      if (event.value == 2) {
        pendingLetters = state.letters;
        view = View::Lengths;
      }
      ready = false;
    } else if (event.action == wordleui::ActionLength && view == View::Lengths && event.value >= 4 &&
               event.value <= 6) {
      pendingLetters = event.value;
      ready = false;
    } else if (event.action == wordleui::ActionApplyLength && view == View::Lengths) {
      if (state.letters != pendingLetters) {
        state.letters = pendingLetters;
        dirty = true;
        save();
      }
      view = View::Menu;
      ready = false;
    } else if (event.action == wordleui::ActionConfirm && view == View::ConfirmNew) {
      play(true);
      return;
    } else if (event.action == wordleui::ActionNext) {
      play();
      return;
    } else if (event.action == wordleui::ActionArchive) {
      view = View::Menu;
      ready = false;
    }
    if (event.action) requestUpdate();
  }
  if (dirty && millis() - lastSave >= 2000) save();
}
void WordleActivity::render(RenderLock&&) {
  if (view == View::Lengths) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, false);
  const freeink::ui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  switch (view) {
    case View::Menu: {
      wordleui::MenuModel model;
      model.record = &state.record;
      model.letters = state.letters;
      model.resume = state.gameLanguage == state.language && state.active && !state.game.over() &&
                     state.game.letters() == state.letters;
      model.state = saveFailed ? tr(STR_WORDLE_SAVE_FAILED) : "";
      wordleui::buildMenu(screen, model, work);
      break;
    }
    case View::Game: {
      char label[20];
      std::snprintf(label, sizeof(label), tr(STR_WORDLE_LETTERS), state.game.letters());
      keys = wordleui::buildGame(screen, {&state.game, label, message, true}, work);
      break;
    }
    case View::HowTo:
      wordleui::buildHowTo(screen);
      break;
    case View::Lengths:
      wordleui::buildLengths(screen, pendingLetters, work);
      break;
    case View::ConfirmNew:
      wordleui::buildConfirm(screen, work);
      break;
  }
  toybox::reportOverflow(interactions, "Wordle", &renderer);
  ready = true;
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
