#pragma once
#include <FreeInkUI.h>
#include <I18n.h>

#include <algorithm>

#include "../ui/LightUi.h"
namespace referenceslider {
namespace fui = freeink::ui;
template <typename Screen, typename Spec>
void build(Screen& screen, const Spec& spec, bool touch) {
  auto safe = screen.device().safeRect();
  if (chrome::lightHeader) {
    const int header = std::clamp(chrome::headerHeight - int(safe.y), 0, int(safe.height));
    safe.y += header;
    safe.height -= header;
  }
  const int width = std::min(int(safe.width) - 20, 560);
  const int height = std::min(int(safe.height) - 20, spec.subtitle ? 372 : 336);
  const auto box = fui::centeredRect(safe, fui::Size{int16_t(width), int16_t(height)});
  const auto black = fui::Paint::solid(fui::Color::Black), white = fui::Paint::solid(fui::Color::White);
  if (chrome::lightHeader) lightui::shade(screen.target(), screen.device().screen());
  screen.target().fill(box, white, chrome::lightHeader ? 8 : 0);
  screen.target().stroke(box, black, chrome::lightHeader ? 1 : 3, chrome::lightHeader ? 8 : 0);
  // The guard must precede the controls: the newest hit region wins.
  screen.frame().hit(box, spec.chromeAction, 0, fui::InputTouch);
  fui::TextStyle text;
  text.font = 0;
  text.bold = !chrome::lightHeader;
  text.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(box.x + 12, box.y + 16, box.width - 24, 42), spec.title, text);
  int y = box.y + 62;
  if (spec.subtitle) {
    screen.target().text(fui::makeRect(box.x + 12, y, box.width - 24, 34), spec.subtitle, text);
    y += 36;
  }
  text.font = chrome::lightHeader ? 0 : 1;
  screen.target().text(fui::makeRect(box.x + 12, y, box.width - 24, 48), spec.readout, text);
  y += 54;
  const auto slider = fui::makeRect(box.x + 30, y + 22, box.width - 60, 44);
  text.font = chrome::lightHeader ? 4 : 3;
  text.align = fui::TextAlign::Left;
  if (spec.minLabel) screen.target().text(fui::makeRect(slider.x - 8, y - 4, slider.width, 30), spec.minLabel, text);
  text.align = fui::TextAlign::Right;
  if (spec.maxLabel) screen.target().text(fui::makeRect(slider.x, y - 4, slider.width + 8, 30), spec.maxLabel, text);
  screen.target().fill(fui::makeRect(slider.x, slider.y + 20, slider.width, 4), black);
  const int center =
      slider.x + int64_t(std::clamp(spec.value, 0, std::max(1, spec.max))) * (slider.width - 1) / std::max(1, spec.max);
  const auto thumb = fui::makeRect(center - 9, slider.y + 12, 18, 20);
  screen.target().fill(thumb, white);
  screen.target().stroke(thumb, black, 2);
  screen.frame().hit(slider, spec.sliderAction, 0, fui::InputTouch | fui::InputDrag);
  // Reused style object stays out of the embedded task stack.
  static fui::ButtonProps button;
  button = {};
  button.styles.explicitlySet = true;
  button.styles.normal.background = white;
  button.styles.normal.foreground = black;
  button.styles.normal.border = black;
  button.styles.normal.borderWidth = 2;
  button.styles.selected = button.styles.active = button.styles.normal;
  if (chrome::lightHeader) button.styles = lightui::controlStyles();
  button.text.font = chrome::lightHeader ? 0 : 1;
  button.text.bold = !chrome::lightHeader;
  button.text.align = fui::TextAlign::Center;
  button.minTouchSize = 0;
  button.action = spec.stepAction;
  const int stepY = box.bottom() - 142;
  for (int n = 0; n < 2; ++n) {
    button.value = n ? 1 : -1;
    button.label = "";
    const auto step = fui::makeRect(box.x + box.width / 2 + (n ? 14 : -82), stepY, 68, 56);
    screen.button(button, step);
    const int x = step.x + step.width / 2, y = step.y + step.height / 2;
    screen.target().line({int16_t(x - 11), int16_t(y)}, {int16_t(x + 11), int16_t(y)}, 3, black);
    if (n) screen.target().line({int16_t(x), int16_t(y - 11)}, {int16_t(x), int16_t(y + 11)}, 3, black);
  }
  button.styles.normal.background = chrome::lightHeader ? white : black;
  button.styles.normal.foreground = chrome::lightHeader ? black : white;
  button.styles.selected = button.styles.active = button.styles.normal;
  button.action = spec.okAction;
  button.value = 0;
  button.label = tr(STR_CONFIRM);
  button.text.font = 0;
  screen.button(button, fui::makeRect(box.x + 18, box.bottom() - 72, box.width - 36, 54));
  (void)touch;  // Physical stepping and Back/Confirm remain owned by the activity.
}
}  // namespace referenceslider
