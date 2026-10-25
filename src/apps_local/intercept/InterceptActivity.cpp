#include "InterceptActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <fontIds.h>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "InterceptArt.h"
#include "components/SideButtonNavigation.h"
namespace fui = freeink::ui;
std::unique_ptr<Activity> InterceptActivity::create(GfxRenderer& r, MappedInputManager& i) {
  auto app = makeUniqueNoThrow<InterceptActivity>(r, i);
  if (!app) LOG_ERR("INTERCEPT", "OOM: %u bytes", unsigned(sizeof(InterceptActivity)));
  return app;
}
void InterceptActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  if (assets.open(interceptart::PATH, interceptart::COUNT)) failed = !intercept::load(state, writable);
  requestUpdate();
}
void InterceptActivity::onExit() {
  assets.close();
  Activity::onExit();
}
void InterceptActivity::back() {
  if (!assets.ready() || view == interceptui::View::Menu)
    shelf::leave(renderer, mappedInput);
  else {
    view = interceptui::View::Menu;
    interactions.setFocusedIndex(-1);
    selected = page = 0;
    ready = false;
    requestUpdate();
  }
}
void InterceptActivity::action(fui::ActionId action, int value) {
  using namespace interceptui;
  if (action == Back) {
    back();
    return;
  }
  if (!assets.ready()) return;
  switch (action) {
    case Play:
      view = View::Story;
      break;
    case New:
      view = View::Restart;
      break;
    case Cancel:
      view = View::Menu;
      break;
    case About:
      view = View::About;
      break;
    case Page: {
      const int next = std::clamp(page + value, 0, std::max(0, work.pages - 1));
      if (next == page) return;
      page = next;
    }
      ready = false;
      requestUpdate();
      return;
    case ConfirmNew: {
      intercept::State draft;
      draft.sequence = state.sequence;
      if (intercept::save(draft, writable)) {
        state = draft;
        view = View::Story;
        failed = false;
      } else
        failed = true;
      break;
    }
    case Choice: {
      auto draft = state;
      if (!intercept::choose(draft, value)) return;
      if (intercept::save(draft, writable)) {
        state = draft;
        failed = false;
      } else
        failed = true;
      break;
    }
    default:
      return;
  }
  selected = page = 0;
  interactions.setFocusedIndex(-1);
  ready = false;
  requestUpdate();
}
void InterceptActivity::loop() {
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    back();
    return;
  }
  if (assets.ready() && ready && interactions.routable() &&
      (view == interceptui::View::Story || view == interceptui::View::About)) {
    if (const int direction = sidebuttons::horizontalStep(mappedInput)) {
      action(interceptui::Page, direction);
      return;
    }
  }
  if (assets.ready() && ready && view == interceptui::View::About) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      action(interceptui::Page, direction);
      return;
    }
  }
  if (assets.ready() && ready && view == interceptui::View::Story) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      action(interceptui::Page, direction);
      return;
    }
  }
  if (!assets.ready() && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    back();
    return;
  }
  if (assets.ready() &&
      (!interactions.hasButtonFocus() && mappedInput.wasReleased(MappedInputManager::Button::Confirm))) {
    if (view == interceptui::View::Menu)
      action(selected == 0 ? interceptui::Play : selected == 1 ? interceptui::New : interceptui::About, 0);
    else if (view == interceptui::View::Restart)
      action(selected == 0 ? interceptui::ConfirmNew : interceptui::Cancel, 0);
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) action(interceptui::Page, -1);
  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) action(interceptui::Page, 1);
  int x, y;
  if (!ready || !sidebuttons::poll(interactions, mappedInput, x, y, !(view == interceptui::View::Story))) return;
  fui::InputSnapshot in;
  in.touchReleased = true;
  in.touchX = x;
  in.touchY = y;
  const auto event = sidebuttons::route(interactions, in, mappedInput, *this, !(view == interceptui::View::Story));
  action(event.action, event.value);
}
void InterceptActivity::render(RenderLock&&) {
  if (view == interceptui::View::Story) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, I18N.getLanguage() != Language::EN);
  target.setFont(4, view == interceptui::View::Story ? toybox::kProseFontId : toybox::kLibraryFontId);
  const fui::InputSnapshot in{};
  toybox::Frame frame(target, target.deviceContext(), in, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  const bool resourcesReady = assets.ready();
  interceptui::build(screen, {&state, view, failed, selected, page, &assets}, work);
  if (resourcesReady && !assets.ready()) {
    renderer.clearScreen();
    interactions.clear();
    interceptui::build(screen, {&state, view, failed, selected, page, &assets}, work);
  }
  ready = true;
  toybox::reportOverflow(interactions, "Intercept", &renderer);
  renderer.displayBuffer();
}
