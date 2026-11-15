#include "TimerSettingsUi.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/Dashboard.h"

namespace timerui {
namespace fui = freeink::ui;
namespace {
void card(toybox::Screen& screen, fui::Rect r) {
  screen.target().stroke(r, dashboard::ink, 2, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
}
void caption(toybox::Screen& screen, fui::Rect r, const char* value, bool strong = false, int lines = 1,
             fui::TextAlign align = fui::TextAlign::Left) {
  fui::TextStyle style;
  style.font = strong ? pixelui::kSmall : 3;
  style.bold = strong;
  style.maxLines = lines;
  style.align = align;
  if (lines == 1 && screen.target().measureText(style.font, value, style).width > r.width) style.font = 3;
  screen.target().text(toybox::inkCentred(screen.target(), r, style), value, style);
}
}  // namespace
void control(toybox::Screen& screen, fui::Rect box, const char* label, fui::ActionId action, int value, bool checked) {
  const bool selected = screen.frame().buttonSelected(action, value, checked);
  if (selected)
    screen.target().fill(box, dashboard::ink, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
  else
    card(screen, box);
  fui::TextStyle style;
  style.font = pixelui::kSmall;
  style.align = fui::TextAlign::Center;
  style.color = selected ? fui::Color::White : fui::Color::Black;
  if (screen.target().measureText(style.font, label, style).width > box.width - 16) style.font = 3;
  screen.target().text(toybox::inkCentred(screen.target(), box.inset({8, 0, 8, 0}), style), label, style);
  dashboard::hit(screen, box, action, value);
}
void confirmation(toybox::Screen& screen, const char* question, const char* confirmLabel, fui::ActionId confirm,
                  fui::ActionId cancel) {
  const auto body = pixelui::geometry(screen.device()).body;
  const int width = std::min(420, int(body.width) - 24);
  fui::TextStyle text;
  text.font = pixelui::kSmall;
  text.align = fui::TextAlign::Center;
  text.maxLines = 3;
  const int questionHeight = fui::measureWrappedText(screen.target(), question, text, width - 40).height;
  const int height = questionHeight + 112;
  const auto panel =
      fui::makeRect(body.x + (body.width - width) / 2, body.y + (body.height - height) / 2, width, height);
  screen.target().fill(panel, dashboard::paper, lightui::radius());
  card(screen, panel);
  screen.target().text(fui::makeRect(panel.x + 20, panel.y + 20, width - 40, questionHeight), question, text);
  const int half = (width - 44) / 2;
  control(screen, fui::makeRect(panel.x + 16, panel.bottom() - 64, half, 48), tr(STR_WORDLE_CANCEL), cancel);
  control(screen, fui::makeRect(panel.x + 28 + half, panel.bottom() - 64, half, 48), confirmLabel, confirm, 0, true);
}
fui::Rect durations(toybox::Screen& screen, fui::Rect body, const pomodoro::Config& config, fui::ActionId change,
                    char* valueText, size_t textSize) {
  const bool wide = screen.device().width > screen.device().height;
  const int columns = wide ? 2 : 1, rows = 4 / columns;
  const int rowHeight = wide ? std::clamp((int(body.height) - 206) / 2, 60, 72) : 72;
  dashboard::icon(screen, fui::makeRect(body.x + 4, body.y + 2, 26, 26), extra_clock_32);
  caption(screen, fui::makeRect(body.x + 42, body.y, body.width - 46, 32), tr(STR_FOCUS_TIMER), true);
  const auto group = fui::makeRect(body.x, body.y + 40, body.width, rows * rowHeight);
  card(screen, group);
  static constexpr StrId names[] = {StrId::STR_FOCUS_CONCENTRATION, StrId::STR_FOCUS_SHORT_BREAK,
                                    StrId::STR_FOCUS_LONG_BREAK, StrId::STR_FOCUS_LONG_EVERY};
  const unsigned values[] = {config.focus, config.shortBreak, config.longBreak, config.rounds};
  for (int n = 0; n < 4; ++n) {
    const int column = n / rows, row = n % rows;
    const auto box = fui::makeRect(group.x + column * group.width / columns, group.y + row * rowHeight,
                                   group.width / columns, rowHeight);
    if (row)
      screen.target().line({int16_t(box.x + 1), box.y}, {int16_t(box.right() - 1), box.y}, 1,
                           fui::Paint::dither(fui::Color::DarkGray));
    if (column && row == 0)
      screen.target().line({box.x, int16_t(group.y + 1)}, {box.x, int16_t(group.bottom() - 1)}, 1,
                           fui::Paint::dither(fui::Color::DarkGray));
    const auto name = fui::makeRect(box.x + 18, box.y + 4, box.width - 238, box.height - 8);
    if (wide) {
      fui::TextStyle nameStyle;
      nameStyle.font = 3;
      nameStyle.bold = true;
      const char* value = I18N.get(names[n]);
      nameStyle.maxLines = screen.target().measureText(nameStyle.font, value, nameStyle).width > name.width ? 2 : 1;
      const int height = nameStyle.maxLines * screen.target().lineHeight(nameStyle.font);
      screen.target().text(fui::makeRect(name.x, box.y + (box.height - height) / 2, name.width, height), value,
                           nameStyle);
    } else {
      caption(screen, name, I18N.get(names[n]), true);
    }
    const int controlsY = box.y + (box.height - 44) / 2;
    const auto minus = fui::makeRect(box.right() - 206, controlsY, 44, 44);
    const auto plus = fui::makeRect(box.right() - 56, controlsY, 44, 44);
    control(screen, minus, "-", change, n * 2);
    control(screen, plus, "+", change, n * 2 + 1);
    const int valueX = box.right() - 150;
    const int valueY = box.y + (box.height - 56) / 2;
    std::snprintf(valueText, textSize, "%u", values[n]);
    caption(screen, fui::makeRect(valueX, valueY, 88, 30), valueText, true, 1, fui::TextAlign::Center);
    caption(screen, fui::makeRect(valueX, valueY + 32, 88, 24),
            n == 3 ? tr(STR_FOCUS_SESSION_UNIT) : tr(STR_STATS_UNIT_MIN), false, 1, fui::TextAlign::Center);
  }
  return group;
}
}  // namespace timerui
