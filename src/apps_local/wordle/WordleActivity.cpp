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
  localized =
      I18N.getLanguage() != Language::EN && I18N.getLanguage() != Language::ES && I18N.getLanguage() != Language::OOK;
  // An unfinished resident game also accepts guesses from its SD vocabulary.
  if (!localized && I18N.getLanguage() != Language::OOK) {
    if (state.active && !state.game.over()) {
      native.open(state.gameLanguage ? Language::ES : Language::EN);
    } else {
      localized = native.open(I18N.getLanguage());
      if (localized) native.inheritStatistics(state.record);
    }
  }
  if (localized) {
    if (!native.available()) native.open(I18N.getLanguage());
    saveFailed = native.persistenceFailed();
    if (native.active()) state.letters = native.letters();
    if (!native.supports(state.letters))
      for (unsigned n = 2; n <= 6; ++n)
        if (native.supports(n)) {
          state.letters = n;
          break;
        }
  }
  if (state.seed == 1 && !state.active && !state.record.played) state.seed = esp_random();
  lastSave = millis();
  requestUpdate();
}
void WordleActivity::save() {
  if (!dirty || localized) return;
  saveFailed = !store.save(state);
  if (!saveFailed) dirty = false;
  lastSave = millis();
}
void WordleActivity::prepareForSleep() { save(); }
void WordleActivity::onExit() {
  save();
  keyboard.close();
  native.close();
  Activity::onExit();
}
void WordleActivity::play(bool replace) {
  if (localized) {
    if (!native.available()) {
      message = tr(STR_LANGUAGE_UNAVAILABLE);
      requestUpdate();
      return;
    }
    if (native.active() && !native.over() && native.letters() != state.letters && !replace)
      view = View::ConfirmNew;
    else {
      if (!keyboard.available() && !keyboard.begin(I18N.getLanguage())) {
        message = tr(STR_LANGUAGE_UNAVAILABLE);
        requestUpdate();
        return;
      }
      if ((replace || !native.active() || native.over()) && !native.start(state.letters, esp_random())) return;
      keyboard.clear();
      keyboard.wordGame(native.letters());
      view = View::Game;
      saveFailed = native.persistenceFailed();
      message = saveFailed ? tr(STR_WORDLE_SAVE_FAILED) : nullptr;
    }
    ready = false;
    requestUpdate();
    return;
  }
  if (state.active && !state.game.over() &&
      (state.game.letters() != state.letters || state.gameLanguage != state.language) && !replace)
    view = View::ConfirmNew;
  else {
    if (replace || !state.active || state.game.over()) {
      localwordle::record(state, replace);
      localwordle::start(state);
      if (I18N.getLanguage() != Language::OOK) native.open(state.gameLanguage ? Language::ES : Language::EN);
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
        auto& activity = *static_cast<WordleActivity*>(context);
        const auto& game = activity.state.game;
        return localwordle::contains(game.letters(), word, activity.state.gameLanguage) ||
               activity.native.acceptsBuiltin(game.letters(), word, activity.state.gameLanguage) ||
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
  const bool typing = ready && localized && view == View::Game && !native.over();
  if (typing) keyboard.capture(mappedInput);
  RenderLock lock(typing ? RenderLock::Mode::Try : RenderLock::Mode::Blocking);
  if (!lock.ownsLock()) return;
  if (typing) {
    const auto key = keyboard.poll(mappedInput, false);
    if (key != EmbeddedKeyboard::Event::None) {
      if (key == EmbeddedKeyboard::Event::Cancel) {
        view = View::Menu;
        keyboard.close();
        ready = false;
        requestUpdate();
        return;
      }
      if (key == EmbeddedKeyboard::Event::Changed) {
        message = nullptr;
        requestUpdate();
      }
      if (key == EmbeddedKeyboard::Event::Submit) {
        const bool accepted = native.submit(keyboard.text());
        saveFailed = native.persistenceFailed();
        message = !accepted ? tr(STR_WORDLE_UNKNOWN) : saveFailed ? tr(STR_WORDLE_SAVE_FAILED) : nullptr;
        if (accepted) keyboard.clear();
        requestUpdate();
      }
      return;
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    save();
    if (view == View::Menu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    view = View::Menu;
    keyboard.close();
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
    if (event.action == wordleui::ActionKeyboard && view == View::Game && !localized) {
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
    } else if (event.action == wordleui::ActionLength && view == View::Lengths && event.value >= (localized ? 2 : 4) &&
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
      if (localized) {
        model.localized = &native;
        model.record = &native.statistics();
      }
      model.letters = state.letters;
      model.language = state.language;
      model.resume = state.gameLanguage == state.language && state.active && !state.game.over() &&
                     state.game.letters() == state.letters;
      model.state = saveFailed ? tr(STR_WORDLE_SAVE_FAILED) : "";
      if (localized && !native.available()) model.state = tr(STR_LANGUAGE_UNAVAILABLE);
      if (localized) model.resume = native.active() && !native.over() && native.letters() == state.letters;
      wordleui::buildMenu(screen, model, work);
      break;
    }
    case View::Game: {
      if (localized) {
        wordleui::buildNativeGame(screen, native, message, work, keyboard);
        break;
      }
      char label[20];
      std::snprintf(label, sizeof(label), tr(STR_WORDLE_LETTERS), state.game.letters());
      keys = wordleui::buildGame(screen, {&state.game, label, message, true, state.gameLanguage}, work);
      break;
    }
    case View::HowTo:
      wordleui::buildHowTo(screen, localized);
      break;
    case View::Lengths:
      if (localized)
        wordleui::buildNativeLengths(screen, native, pendingLetters, work);
      else
        wordleui::buildLengths(screen, pendingLetters, work);
      break;
    case View::ConfirmNew:
      wordleui::buildConfirm(screen, work);
      break;
  }
  toybox::reportOverflow(interactions, "Wordle", &renderer);
  ready = true;
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  if (localized && view == View::Game) keyboard.didPaint();
}
