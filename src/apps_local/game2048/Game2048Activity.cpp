#include "Game2048Activity.h"

#include <I18n.h>
#include <Memory.h>
#include <esp_random.h>

#include <algorithm>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "components/SideButtonNavigation.h"
namespace fui = freeink::ui;
std::unique_ptr<Activity> Game2048Activity::create(GfxRenderer& renderer, MappedInputManager& input) {
  auto result = makeUniqueNoThrow<Game2048Activity>(renderer, input);
  if (!result) LOG_ERR("2048", "OOM: activity");
  return result;
}
void Game2048Activity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  saveFailed = !repository.load(state, best);
  if (!game2048::highest(state)) {
    game2048::start(state, esp_random());
    dirty = true;
  }
  lastSave = millis();
  requestUpdate();
}
void Game2048Activity::save() {
  if (dirty) {
    saveFailed = !repository.save(state, best);
    if (!saveFailed) dirty = false;
    lastSave = millis();
  }
}
void Game2048Activity::prepareForSleep() { save(); }
void Game2048Activity::onExit() {
  save();
  Activity::onExit();
}
void Game2048Activity::action(fui::ActionId action, int value) {
  switch (action) {
    case game2048ui::Play:
      view = game2048ui::View::Board;
      break;
    case game2048ui::New:
      view = game2048ui::View::Confirm;
      selected = 0;
      break;
    case game2048ui::Help:
      view = game2048ui::View::Help;
      break;
    case game2048ui::ConfirmNew:
      game2048::start(state, esp_random());
      dirty = true;
      view = game2048ui::View::Board;
      save();
      break;
    case game2048ui::CancelNew:
      view = game2048ui::View::Menu;
      selected = 0;
      break;
    case game2048ui::Move:
      if (view != game2048ui::View::Board || value < 0 || value > 3) return;
      if (!game2048::move(state, static_cast<game2048::Direction>(value))) return;
      best = std::max(best, state.score);
      dirty = true;
      break;
    default:
      return;
  }
  ready = false;
  requestUpdate();
}
bool Game2048Activity::routeBoardSwipe() {
  // Board swipes own all four directions; the header arrow still handles Back.
  switch (mappedInput.wasSwipe()) {
    case MappedInputManager::SwipeDir::Up:
      action(game2048ui::Move, 0);
      return true;
    case MappedInputManager::SwipeDir::Right:
      action(game2048ui::Move, 1);
      return true;
    case MappedInputManager::SwipeDir::Down:
      action(game2048ui::Move, 2);
      return true;
    case MappedInputManager::SwipeDir::Left:
      action(game2048ui::Move, 3);
      return true;
    case MappedInputManager::SwipeDir::None:
      return false;
  }
  return false;
}
void Game2048Activity::loop() {
  RenderLock lock(*this);
  if (view == game2048ui::View::Board && ready && routeBoardSwipe()) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    save();
    if (view == game2048ui::View::Menu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    view = game2048ui::View::Menu;
    selected = 0;
    ready = false;
    requestUpdate();
    return;
  }
  if (dirty && millis() - lastSave >= 30000) {
    save();
    if (saveFailed) requestUpdate();
  }
  if (view == game2048ui::View::Menu || view == game2048ui::View::Confirm) {
    const int count = view == game2048ui::View::Menu ? 3 : 2;
    if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
      selected = (selected + 1) % count;
      requestUpdate();
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
      selected = (selected + count - 1) % count;
      requestUpdate();
      return;
    }
    if ((!interactions.hasButtonFocus() && mappedInput.wasReleased(MappedInputManager::Button::Confirm))) {
      action(view == game2048ui::View::Menu ? (selected == 0   ? game2048ui::Play
                                               : selected == 1 ? game2048ui::New
                                                               : game2048ui::Help)
                                            : (selected == 0 ? game2048ui::ConfirmNew : game2048ui::CancelNew),
             0);
      return;
    }
  }
  int x, y;
  if (!ready || !sidebuttons::poll(interactions, mappedInput, x, y, view != game2048ui::View::Board)) return;
  fui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = x;
  input.touchY = y;
  const auto event = sidebuttons::route(interactions, input, mappedInput, *this, view != game2048ui::View::Board);
  action(event.action, event.value);
}
void Game2048Activity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  const fui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  game2048ui::build(screen, {&state, best, view, saveFailed, selected}, uiWork);
  ready = true;
  toybox::reportOverflow(interactions, "2048", &renderer);
  renderer.displayBuffer();
}
