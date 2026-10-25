#include <FreeInkApp.h>
#include <FreeInkUI.h>

#include <cassert>
#include <cstdio>

namespace fui = freeink::ui;

struct Target final : fui::DrawTarget {
  fui::Color background = fui::Color::White, foreground = fui::Color::Black;
  int borderWidth = 0;
  fui::Size measureText(fui::FontId, const char*, fui::TextStyle) const override { return {20, 16}; }
  int16_t lineHeight(fui::FontId) const override { return 16; }
  void fill(fui::Rect, fui::Paint paint, uint8_t, uint8_t) override { background = paint.color; }
  void stroke(fui::Rect, fui::Paint, uint8_t width, uint8_t, uint8_t) override { borderWidth = width; }
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void text(fui::Rect, const char*, fui::TextStyle style) override { foreground = style.color; }
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override {}
};

int main() {
  fui::InteractionBuffer<8> hits;
  const auto add = [&](int y, int value, uint16_t mask = fui::InputTouch) {
    hits.addInteraction({fui::makeRect(20, y, 200, 44), 1, static_cast<int16_t>(value), mask});
  };
  add(300, 3);  // Footers may be registered before the content above them.
  add(100, 1);
  add(200, 2);
  fui::InputSnapshot next;
  next.focusNext = true;
  hits.route(next);
  assert(hits.focusedIndex() == 1);
  assert(hits.buttonSelected(1, 1));
  assert(!hits.buttonSelected(1, 3, true));
  hits.route(next);
  assert(hits.focusedIndex() == 2);
  fui::InputSnapshot previous;
  previous.focusPrev = true;
  hits.route(previous);
  assert(hits.focusedIndex() == 1);

  // A repaint keeps the chosen action/value even when registration order changes.
  hits.clear();
  add(100, 1);
  add(200, 2);
  assert(fui::hasState(hits.stateFor(1, 1, fui::StateNormal), fui::StateFocused));
  assert(!fui::hasState(hits.stateFor(1, 2, fui::StateSelected), fui::StateSelected));
  hits.route(next);
  assert(hits.buttonSelected(1, 2));
  hits.route(previous);
  assert(hits.buttonSelected(1, 1));

  Target target;
  fui::DeviceContext device;
  device.width = 480;
  device.height = 800;
  fui::InputSnapshot none;
  fui::Frame<8> frame(target, device, none, hits);
  fui::ButtonProps button;
  button.label = "One";
  button.action = 1;
  button.value = 1;
  fui::button(frame, fui::makeRect(20, 100, 200, 44), button);
  assert(target.background == fui::Color::Black);
  assert(target.foreground == fui::Color::White);
  button.value = 2;
  button.state = fui::StateSelected;
  fui::button(frame, fui::makeRect(20, 200, 200, 44), button);
  assert(target.background == fui::Color::White);
  assert(target.foreground == fui::Color::Black);

  // A header chip owns its explicit palette, even while another control has focus.
  button.value = 3;
  button.state = fui::StateNormal;
  button.styles.explicitlySet = true;
  button.styles.normal.background = fui::Paint::solid(fui::Color::Black);
  button.styles.normal.foreground = fui::Paint::solid(fui::Color::White);
  button.styles.normal.border = fui::Paint::solid(fui::Color::White);
  button.styles.normal.borderWidth = 2;
  fui::button(frame, fui::makeRect(20, 300, 200, 44), button);
  assert(target.background == fui::Color::Black);
  assert(target.foreground == fui::Color::White);
  assert(target.borderWidth == 2);

  // Inheriting the core theme does not turn an ordinary Cancel button into a primary action.
  fui::InteractionBuffer<8> coreHits;
  fui::Frame<8> coreFrame(target, device, none, coreHits);
  fui::ThemeTokens theme;
  theme.button = fui::defaultButtonStyles();
  theme.button.normal.border = fui::Paint::solid(fui::Color::Black);
  theme.button.normal.borderWidth = 1;
  fui::Screen<8> coreScreen(coreFrame, theme);
  fui::ButtonProps cancel;
  cancel.label = "Cancel";
  cancel.action = 20;
  coreScreen.button(cancel, fui::makeRect(20, 100, 200, 44));
  assert(target.background == fui::Color::White);
  assert(target.foreground == fui::Color::Black);
  assert(target.borderWidth == 1);
  coreScreen.button(cancel, fui::makeRect(20, 200, 200, 44), fui::StateSelected);
  assert(target.background == fui::Color::Black);
  assert(target.foreground == fui::Color::White);

  // Touch clears hardware focus, and restricting hardware actions preserves taps.
  hits.limitButtonActions({2});
  hits.route(next);
  assert(!hits.hasButtonFocus());
  fui::InputSnapshot tap;
  tap.touchReleased = true;
  tap.touchX = 40;
  tap.touchY = 120;
  assert(hits.route(tap).action == 1);

  // An obsolete index cannot carry a coordinate cursor into a different screen.
  hits.clear();
  hits.addInteraction({fui::makeRect(20, 100, 200, 44), 5, 0, fui::InputTouch});
  hits.route(previous);
  assert(hits.focusedIndex() == 0);
  std::puts("button-navigation: PASS");
}
