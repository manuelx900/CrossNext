#include "PomodoroScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/Dashboard.h"
#include "../ui/PixelMenu.h"
namespace pomodoroui {
namespace fui = freeink::ui;
void drawCountdown(fui::DrawTarget& target, fui::Rect box, const char* value, int maxInkHeight) {
  if (!value || box.empty()) return;
  constexpr int digitAdvance = numerals::kDigitAdvance, digitHeight = numerals::kDigitHeight;
  int total = 0;
  for (const char* p = value; *p; ++p) {
    if (*p != ':' && (*p < '0' || *p > '9')) return;
    total += *p == ':' ? numerals::kColonAdvance : digitAdvance;
  }
  if (!total) return;
  const int scale = std::min(int(box.width) * 1024 / total,
                             std::min(std::clamp(maxInkHeight, 1, 96), int(box.height)) * 1024 / digitHeight);
  const int x = box.x + (box.width - total * scale / 1024) / 2;
  const int y = box.y + (box.height - digitHeight * scale / 1024) / 2;
  int advance = 0;
  for (const char* p = value; *p; ++p) {
    const auto& glyph = *numerals::glyph(static_cast<unsigned char>(*p));
    const int width = *p == ':' ? numerals::kColonAdvance : digitAdvance;
    const char run[] = {*p, 0};
    numerals::draw(target,
                   fui::makeRect(x + advance * scale / 1024, y + (digitHeight - glyph.top) * scale / 1024,
                                 width * scale / 1024, glyph.height * scale / 1024),
                   run);
    advance += width;
  }
}
namespace {
constexpr auto ink = fui::Paint::solid(fui::Color::Black);
void button(toybox::Screen& screen, Work& work, fui::Rect box, const char* label, fui::ActionId action, int value = 0,
            bool checked = false) {
  work.button = {};
  auto& p = work.button;
  p.styles = pixelui::buttonStyles();
  p.label = label;
  p.text.font = pixelui::kSmall;
  p.text.align = fui::TextAlign::Center;
  p.action = action;
  p.value = value;
  p.minTouchSize = 0;
  p.state = checked ? fui::StateChecked : fui::StateNormal;
  screen.button(p, box);
}
const char* phaseName(pomodoro::Phase phase) {
  switch (phase) {
    case pomodoro::Phase::Focus:
      return tr(STR_POMODORO_STAGE_FOCUS);
    case pomodoro::Phase::ShortBreak:
      return tr(STR_UI_SHORT_BREAK_1AD62D);
    case pomodoro::Phase::LongBreak:
      return tr(STR_UI_LONG_BREAK_9921A4);
  }
  return "";
}
}  // namespace
void build(toybox::Screen& screen, const Model& model, Work& work) {
  const auto& s = *model.state;
  pixelui::Header header;
  header.title = "POMODORO";
  header.back = true;
  pixelui::header(screen.target(), screen.device(), header);
  backui::publish(header.back, pixelui::geometry(screen.device()).back);
  auto body = pixelui::geometry(screen.device()).body;
  const bool wide = body.width > body.height;
  if (model.saveFailed) {
    pixelui::text(screen.target(), fui::makeRect(body.x, body.y, body.width, 36), tr(STR_WORDLE_SAVE_FAILED), 3);
    body.y += 40;
    body.height -= 40;
  }
  if (model.view == View::ConfirmReset) {
    fui::TextStyle text;
    text.font = pixelui::kBody;
    text.maxLines = 4;
    screen.target().text(fui::makeRect(body.x + 12, body.y + 24, body.width - 24, 180),
                         tr(STR_UI_RESTART_THIS_TIMER_COMPLETED_FOCUS_SESSIONS_39F159), text);
    button(screen, work, fui::makeRect(body.x, body.bottom() - 140, body.width, 60), tr(STR_UI_RESTART_TIMER_D251E8),
           Confirm);
    button(screen, work, fui::makeRect(body.x, body.bottom() - 68, body.width, 60), tr(STR_WORDLE_CANCEL), Cancel);
    return;
  }
  if (model.view == View::Settings) {
    static constexpr UiLabel names[] = {
        trLabel(STR_UI_FOCUS_MINUTES_2957E3),     trLabel(STR_UI_SHORT_BREAK_1AD62D),
        trLabel(STR_UI_LONG_BREAK_9921A4),        trLabel(STR_UI_LONG_BREAK_EVERY_EA5E55),
        trLabel(STR_UI_AUTO_START_BREAKS_BA074F), trLabel(STR_UI_AUTO_START_FOCUS_A9AA65)};
    const int values[] = {s.config.focus,  s.config.shortBreak, s.config.longBreak,
                          s.config.rounds, s.config.autoBreak,  s.config.autoFocus};
    const int cols = wide ? 2 : 1, rows = wide ? 3 : 6;
    const int rowH = (body.height - 82) / rows, colW = (body.width - (cols - 1) * 12) / cols;
    for (int n = 0; n < 6; ++n) {
      const int x = body.x + (n / rows) * (colW + 12), y = body.y + (n % rows) * rowH;
      if (n % rows == 0)
        lightui::outline(screen.target(), fui::makeRect(x, y, colW, rowH * rows), ink, pixelui::kBorder);
      else
        screen.target().fill(fui::makeRect(x, y, colW, pixelui::kBorder), ink);
      pixelui::text(screen.target(), fui::makeRect(x + 12, y + 5, colW - 24, 32), names[n], 3);
      const int bottom = y + rowH - 56;
      if (n < 4) {
        button(screen, work, fui::makeRect(x + 12, bottom, 48, 44), "-", Change, n * 2);
        std::snprintf(work.label, sizeof(work.label), n == 3 ? tr(STR_UI_D_SESSIONS_2652EC) : "%d MIN", values[n]);
        pixelui::text(screen.target(), fui::makeRect(x + 66, bottom, colW - 132, 44), work.label, pixelui::kSmall,
                      fui::Color::Black, fui::TextAlign::Center);
        button(screen, work, fui::makeRect(x + colW - 60, bottom, 48, 44), "+", Change, n * 2 + 1);
      } else
        button(screen, work, fui::makeRect(x + 12, bottom, colW - 24, 44),
               values[n] ? tr(STR_UI_ON_387D7A) : tr(STR_UI_OFF_AD5048), Change, n * 2, values[n]);
    }
    button(screen, work, fui::makeRect(body.x, body.bottom() - 64, body.width, 60), tr(STR_UI_DONE_E782E7), Done);
    return;
  }
  const int timelineH = chrome::lightHeader ? (wide ? 48 : 60) : wide ? 56 : 80;
  const auto timeline = fui::makeRect(body.x, body.y, body.width, timelineH);
  const int stages = 2 * s.config.rounds, page = std::clamp(model.stagePage, 0, (stages - 1) / 3);
  const int border = chrome::lightHeader ? 1 : pixelui::kBorder;
  const auto previous = fui::makeRect(timeline.x + border, timeline.y + border, 46, timeline.height - 2 * border);
  const auto next = fui::makeRect(timeline.right() - 46 - border, previous.y, 46, previous.height);
  pixelui::text(screen.target(), previous, "<", 0, fui::Color::Black, fui::TextAlign::Center);
  pixelui::text(screen.target(), next, ">", 0, fui::Color::Black, fui::TextAlign::Center);
  screen.frame().hit(previous, StagePage, -1, fui::InputTouch);
  screen.frame().hit(next, StagePage, 1, fui::InputTouch);
  const int stageLeft = previous.right() + border, stageWidth = next.x - border - stageLeft;
  const int visibleStages = std::min(3, stages - page * 3);
  for (int n = 0; n < visibleStages; ++n) {
    const int index = page * 3 + n;
    const auto phase = pomodoro::phaseAt(s.config, index);
    std::snprintf(work.label, sizeof(work.label), tr(STR_POMODORO_STAGE_FORMAT), index / 2 + 1,
                  phase == pomodoro::Phase::Focus       ? tr(STR_POMODORO_STAGE_FOCUS)
                  : phase == pomodoro::Phase::LongBreak ? tr(STR_POMODORO_STAGE_LONG)
                                                        : tr(STR_POMODORO_STAGE_BREAK),
                  static_cast<unsigned long>(pomodoro::duration(s.config, phase) / 60000));
    const int left = stageLeft + stageWidth * n / visibleStages;
    const int right = stageLeft + stageWidth * (n + 1) / visibleStages;
    const auto cell = fui::makeRect(left, previous.y, right - left, previous.height);
    work.button = {};
    work.button.styles = pixelui::buttonStyles();
    work.button.borderEdges = fui::EdgesNone;
    if (chrome::lightHeader) {
      work.button.styles.normal.radius = work.button.styles.selected.radius = work.button.styles.focused.radius =
          work.button.styles.active.radius = 0;
    }
    work.button.label = nullptr;
    work.button.text.font = 3;
    work.button.text.maxLines = 2;
    work.button.text.align = fui::TextAlign::Center;
    work.button.action = SelectStage;
    work.button.value = index;
    work.button.state = index == s.stage ? fui::StateSelected : fui::StateNormal;
    work.button.minTouchSize = 0;
    screen.button(work.button, cell);
    char* minutesLabel = std::strchr(work.label, '\n');
    if (minutesLabel) *minutesLabel++ = 0;
    const auto state = screen.frame().stateFor(SelectStage, index, work.button.state);
    const bool inverse = fui::hasState(state, fui::StateSelected) || fui::hasState(state, fui::StateFocused) ||
                         fui::hasState(state, fui::StateActive);
    const auto color = inverse ? fui::Color::White : fui::Color::Black;
    pixelui::text(screen.target(),
                  fui::makeRect(cell.x + 3,
                                cell.y + (chrome::lightHeader ? (wide ? 1 : 4)
                                          : wide              ? 2
                                                              : 8),
                                cell.width - 6, wide ? 20 : 24),
                  work.label, 3, color, fui::TextAlign::Center);
    if (minutesLabel)
      pixelui::text(screen.target(),
                    fui::makeRect(cell.x + 3,
                                  cell.y + (chrome::lightHeader ? (wide ? 23 : 30)
                                            : wide              ? 24
                                                                : 36),
                                  cell.width - 6, wide ? 20 : 24),
                    minutesLabel, 3, color, fui::TextAlign::Center);
  }
  lightui::outline(screen.target(), timeline, ink, border);
  screen.target().fill(fui::makeRect(previous.right(), previous.y, border, previous.height), ink);
  screen.target().fill(fui::makeRect(next.x - border, previous.y, border, previous.height), ink);
  for (int n = 1; n < visibleStages; ++n)
    screen.target().fill(fui::makeRect(stageLeft + stageWidth * n / visibleStages, previous.y, border, previous.height),
                         ink);
  body.y += timelineH + 12;
  body.height -= timelineH + 12;
  const auto timer = wide ? fui::makeRect(body.x, body.y, body.width - 244, body.height)
                          : fui::makeRect(body.x, body.y, body.width, body.height - 238);
  dashboard::frame(screen, timer, pixelui::kBorder);
  dashboard::text(screen, fui::makeRect(timer.x + 8, timer.y + 8, timer.width - 16, 40), phaseName(s.phase), 2);
  std::snprintf(work.label, sizeof(work.label), tr(STR_POMO_SESSION), int(s.stage / 2 + 1));
  dashboard::text(screen, fui::makeRect(timer.x + 8, timer.y + 48, timer.width - 16, 30), work.label, 3);
  const uint32_t seconds = (s.remainingMs + 999) / 1000;
  std::snprintf(work.label, sizeof(work.label), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60),
                static_cast<unsigned long>(seconds % 60));
  const int countdownTop = timer.y + 80;
  const int noticeTop = timer.bottom() - 116;
  drawCountdown(screen.target(),
                fui::makeRect(timer.x + 18, countdownTop, timer.width - 36, std::max(1, noticeTop - countdownTop - 8)),
                work.label, 96);
  dashboard::text(screen, fui::makeRect(timer.x + 8, noticeTop, timer.width - 16, 34),
                  s.notice    ? tr(STR_UI_TIME_FOR_A_NEW_SESSION_305DE9)
                  : s.running ? tr(STR_UI_IN_PROGRESS_BF8144)
                              : tr(STR_UI_READY_WHEN_YOU_ARE_31EF54),
                  3);
  const uint32_t full = pomodoro::duration(s.config, s.phase);
  dashboard::bar(screen, fui::makeRect(timer.x + 16, timer.bottom() - 70, timer.width - 32, 18),
                 full - std::min(full, s.remainingMs), full);
  std::snprintf(work.label, sizeof(work.label), tr(STR_UI_LU_FOCUS_SESSIONS_D61842),
                static_cast<unsigned long>(s.completed));
  dashboard::text(screen, fui::makeRect(timer.x + 8, timer.bottom() - 42, timer.width - 16, 34), work.label, 3);
  const auto controls =
      wide ? fui::makeRect(timer.right() + 16, body.y, 228, body.height)
           : fui::makeRect(body.x, timer.bottom() + 14, body.width, body.bottom() - timer.bottom() - 14);
  const int h = (controls.height - 20) / 3, half = (controls.width - 10) / 2;
  dashboard::button(screen, fui::makeRect(controls.x, controls.y, controls.width, h),
                    s.running ? tr(STR_UI_PAUSE_9935DF) : tr(STR_UI_START_7196E7), Toggle, 0, true);
  dashboard::button(screen, fui::makeRect(controls.x, controls.y + h + 10, half, h), tr(STR_UI_SKIP_431DE8), Skip);
  dashboard::button(screen, fui::makeRect(controls.x + half + 10, controls.y + h + 10, half, h),
                    tr(STR_UI_RESTART_69ED00), Reset);
  dashboard::button(screen, fui::makeRect(controls.x, controls.y + 2 * (h + 10), controls.width, h),
                    tr(STR_UI_SETTINGS_C4B924), Settings);
}
}  // namespace pomodoroui
