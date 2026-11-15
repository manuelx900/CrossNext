#include "CalculatorActivity.h"

#include <Logging.h>
#include <Memory.h>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "CalculatorScreens.h"
#include "components/SideButtonNavigation.h"

std::unique_ptr<Activity> CalculatorActivity::create(GfxRenderer& renderer, MappedInputManager& input) {
  // The fixed-precision engine and hit table live only while this activity is open.
  auto activity = makeUniqueNoThrow<CalculatorActivity>(renderer, input);
  if (!activity) LOG_ERR("CALC", "OOM: CalculatorActivity");
  return activity;
}
void CalculatorActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  requestUpdate();
}
void CalculatorActivity::render(RenderLock&&) {
  ready = false;
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const freeink::ui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions);
  toybox::Screen screen(frame);
  calculatorui::build(screen, engine);
  toybox::reportOverflow(interactions, "Calculator", &renderer);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
  ready = true;
}
void CalculatorActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  int x = 0, y = 0;
  if (!ready || !interactions.routable() || !sidebuttons::poll(interactions, mappedInput, x, y)) return;
  freeink::ui::InputSnapshot input{};
  input.touchReleased = sidebuttons::hasActivation(x, y);
  input.touchX = x;
  input.touchY = y;
  sidebuttons::route(interactions, input, mappedInput, *this);
  if (!sidebuttons::hasActivation(x, y)) return;
  auto target = toybox::makeTarget(renderer);
  const auto pad = calculatorui::geometry(target.deviceContext());
  const int index = calc::keyAt(calc::kPad, pad, x, y);
  if (index < 0) return;
  engine.press(calc::kPad.keys[index].key);
  ready = false;
  requestUpdate();
}
