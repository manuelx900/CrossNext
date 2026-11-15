#include "PomodoroScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/Dashboard.h"
#include "../ui/PixelMenu.h"
#include "../ui/ToyboxIcons.h"
#include "TimerSettingsUi.h"

namespace pomodoroui {
namespace fui = freeink::ui;
void drawCountdown(fui::DrawTarget& target, fui::Rect box, const char* value, int maxInkHeight) {
  if (!value || box.empty()) return;
  const int digitAdvance = numerals::digitAdvance(), digitHeight = numerals::digitHeight();
  int total = 0;
  for (const char* p = value; *p; ++p) {
    if (*p != ':' && (*p < '0' || *p > '9')) return;
    total += *p == ':' ? numerals::colonAdvance() : digitAdvance;
  }
  if (!total) return;
  const int scale = std::min(int(box.width) * 1024 / total,
                             std::min(std::clamp(maxInkHeight, 1, 160), int(box.height)) * 1024 / digitHeight);
  const int x = box.x + (box.width - total * scale / 1024) / 2;
  const int y = box.y + (box.height - digitHeight * scale / 1024) / 2;
  int advance = 0;
  for (const char* p = value; *p; ++p) {
    const auto& glyph = *numerals::glyph(static_cast<unsigned char>(*p));
    const int width = *p == ':' ? numerals::colonAdvance() : digitAdvance;
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
void settingsCaption(toybox::Screen& screen, fui::Rect box, const char* value, bool strong = false) {
  fui::TextStyle style;
  style.font = strong ? pixelui::kSmall : 3;
  style.bold = strong;
  if (screen.target().measureText(style.font, value, style).width > box.width) style.font = 3;
  screen.target().text(toybox::inkCentred(screen.target(), box, style), value, style);
}
void automaticSettings(toybox::Screen& screen, fui::Rect area, const pomodoro::Config& config) {
  const bool wide = screen.device().width > screen.device().height;
  const int columns = wide ? 2 : 1, rows = 2 / columns;
  const int rowHeight = wide ? 52 : 64;
  settingsCaption(screen, fui::makeRect(area.x + 4, area.y, area.width - 8, 30), tr(STR_POMODORO_AUTOMATIC), true);
  const auto group = fui::makeRect(area.x, area.y + 40, area.width, rows * rowHeight);
  screen.target().stroke(group, ink, 2, chrome::lightHeader ? 2 * lightui::kControlRadius : 0);
  static constexpr StrId names[] = {StrId::STR_POMODORO_AUTO_BREAKS, StrId::STR_POMODORO_AUTO_FOCUS};
  const bool values[] = {config.autoBreak, config.autoFocus};
  for (int n = 0; n < 2; ++n) {
    const auto row = fui::makeRect(group.x + (n / rows) * group.width / columns, group.y + (n % rows) * rowHeight,
                                   group.width / columns, rowHeight);
    if (n) {
      if (columns == 1)
        screen.target().line({int16_t(row.x + 1), row.y}, {int16_t(row.right() - 1), row.y}, 1,
                             fui::Paint::dither(fui::Color::DarkGray));
      else
        screen.target().line({row.x, int16_t(row.y + 12)}, {row.x, int16_t(row.bottom() - 12)}, 1,
                             fui::Paint::dither(fui::Color::DarkGray));
    }
    settingsCaption(screen, fui::makeRect(row.x + 18, row.y, row.width - 146, row.height), I18N.get(names[n]), true);
    const auto toggle = fui::makeRect(row.right() - 116, row.y + (row.height - 44) / 2, 100, 44);
    timerui::control(screen, toggle, values[n] ? tr(STR_UI_ON_387D7A) : tr(STR_UI_OFF_AD5048), Change, (n + 4) * 2,
                     values[n]);
  }
}
void settings(toybox::Screen& screen, fui::Rect body, const pomodoro::Config& config,
              const pomodoro::DisplayOptions& options, Work& work) {
  const bool wide = screen.device().width > screen.device().height;
  body.x += 8;
  body.width -= 16;
  auto durationArea = body;
  if (wide) durationArea.height -= 64;
  const auto group = timerui::durations(screen, durationArea, config, Change, work.label, sizeof(work.label));
  auto automaticArea = body;
  automaticArea.y = group.bottom() + (wide ? 10 : 24);
  automaticSettings(screen, automaticArea, config);
  const int half = (body.width - 12) / 2;
  const auto visibility = wide ? fui::makeRect(body.x, body.bottom() - 52, half, 52)
                               : fui::makeRect(body.x, body.bottom() - 116, body.width, 52);
  screen.target().stroke(visibility, ink, 2, lightui::radius());
  settingsCaption(screen, fui::makeRect(visibility.x + 16, visibility.y, visibility.width - 146, 52),
                  tr(STR_POMODORO_SHOW_ACCUMULATION), true);
  timerui::control(screen, fui::makeRect(visibility.right() - 116, visibility.y + 4, 100, 44),
                   options.showAccumulation ? tr(STR_UI_ON_387D7A) : tr(STR_UI_OFF_AD5048), ToggleAccumulation, 0,
                   options.showAccumulation);
  const auto done = wide ? fui::makeRect(body.x + half + 12, body.bottom() - 52, body.width - half - 12, 52)
                         : fui::makeRect(body.x + 32, body.bottom() - 52, body.width - 64, 52);
  timerui::control(screen, done, tr(STR_FOCUS_DONE), Done, 0, true);
}
const char* accumulationTitle(pomodoro::Period period) {
  switch (period) {
    case pomodoro::Period::Day:
      return tr(STR_TODAY);
    case pomodoro::Period::Week:
      return tr(STR_POMODORO_THIS_WEEK);
    case pomodoro::Period::Month:
      return tr(STR_POMODORO_THIS_MONTH);
    case pomodoro::Period::Year:
      return tr(STR_POMODORO_THIS_YEAR);
  }
  return "";
}
void accumulationCard(toybox::Screen& screen, fui::Rect box, const Model& model, Work& work) {
  const auto* stats = model.accumulation;
  screen.target().stroke(box, ink, 2, lightui::radius());
  if (model.choosingAccumulation) {
    static constexpr pomodoro::Period periods[] = {pomodoro::Period::Day, pomodoro::Period::Week,
                                                   pomodoro::Period::Month, pomodoro::Period::Year};
    static constexpr StrId labels[] = {StrId::STR_TODAY, StrId::STR_POMODORO_WEEK, StrId::STR_POMODORO_MONTH,
                                       StrId::STR_POMODORO_YEAR};
    for (int n = 0; n < 4; ++n) {
      const auto cell =
          fui::makeRect(box.x + 8 + n % 2 * (box.width - 16) / 2, box.y + 8 + n / 2 * 48, (box.width - 24) / 2, 44);
      timerui::control(screen, cell, I18N.get(labels[n]), SelectAccumulation, int(periods[n]),
                       stats && stats->period == periods[n]);
    }
    return;
  }
  const auto choose = fui::makeRect(box.right() - 56, box.y + (box.height - 44) / 2, 44, 44);
  timerui::control(screen, choose, ">", ChooseAccumulation);
  dashboard::text(screen, fui::makeRect(box.x + 64, box.y + 10, box.width - 128, 28),
                  accumulationTitle(stats ? stats->period : pomodoro::Period::Week), 3, fui::TextAlign::Center);
  const uint64_t minutes = stats ? stats->millisecondsWithout(model.activeCredit) / 60000 : 0;
  if (!model.today || !stats || !stats->first || model.loading || model.historyFailed)
    std::snprintf(work.label, sizeof(work.label), "--");
  else if (minutes >= 60)
    std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_DURATION_HOURS),
                  unsigned(std::min(minutes / 60, uint64_t(UINT32_MAX))), unsigned(minutes % 60));
  else
    std::snprintf(work.label, sizeof(work.label), tr(STR_FOCUS_DURATION_MINUTES), unsigned(minutes));
  const auto valueBox = fui::makeRect(box.x + 64, box.y + 42, box.width - 128, 38);
  fui::TextStyle style;
  style.font = pixelui::kBody;
  style.align = fui::TextAlign::Center;
  if (screen.target().measureText(style.font, work.label, style).width > valueBox.width) style.font = 3;
  screen.target().text(toybox::inkCentred(screen.target(), valueBox, style), work.label, style);
}
}  // namespace
void build(toybox::Screen& screen, const Model& model, Work& work) {
  const auto& s = *model.state;
  static constexpr pomodoro::DisplayOptions defaults{};
  const auto& options = model.options ? *model.options : defaults;
  pixelui::Header header;
  header.title = model.view == View::Settings ? tr(STR_SETTINGS_TITLE)
                 : model.view == View::Stats  ? tr(STR_POMODORO_HISTORY)
                                              : tr(STR_POMODORO_APP);
  header.back = true;
  if (model.view == View::Timer || model.view == View::ConfirmSkip || model.view == View::ConfirmReset) {
    header.mark = fui::bitmapFromIcon(icon_go_settings_32);
    header.markAction = Settings;
    header.secondaryMark = fui::bitmapFromIcon(extra_calendar_32);
    header.secondaryMarkAction = OpenStats;
  }
  pixelui::header(screen, header);
  backui::publish(header.back, pixelui::geometry(screen.device()).back);
  auto body = pixelui::geometry(screen.device()).body;
  const bool wide = body.width > body.height;
  if (model.saveFailed) {
    pixelui::text(screen.target(), fui::makeRect(body.x, body.y, body.width, 36), tr(STR_WORDLE_SAVE_FAILED), 3);
    body.y += 40;
    body.height -= 40;
  }
  if (model.view == View::Stats) {
    statistics(screen, body, model, work);
    return;
  }
  if (model.view == View::Settings) {
    settings(screen, body, s.config, options, work);
    return;
  }
  const auto date = readingstats::dateFromDay(model.today);
  dashboard::date(work.label, sizeof(work.label), date.year, date.month, date.day);
  dashboard::icon(screen, fui::makeRect(body.x + 4, body.y + 5, 26, 26), extra_calendar_32);
  dashboard::text(screen, fui::makeRect(body.x + 40, body.y, body.width - 44, 36), work.label, 3, fui::TextAlign::Left);
  body.y += 46;
  body.height -= 46;
  const int timelineH = wide ? 48 : 64;
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
    const int lineHeight = std::min(24, (int(cell.height) - 12) / 2);
    const int labelTop = cell.y + (cell.height - (minutesLabel ? 2 : 1) * lineHeight) / 2;
    pixelui::text(screen.target(), fui::makeRect(cell.x + 3, labelTop, cell.width - 6, lineHeight), work.label, 3,
                  color, fui::TextAlign::Center);
    if (minutesLabel)
      pixelui::text(screen.target(), fui::makeRect(cell.x + 3, labelTop + lineHeight, cell.width - 6, lineHeight),
                    minutesLabel, 3, color, fui::TextAlign::Center);
  }
  screen.target().stroke(timeline, ink, pixelui::kBorder, lightui::radius());
  // Partial dividers only between unselected neighbours.
  for (int n = 0; n <= visibleStages; ++n) {
    const int leftStage = page * 3 + n - 1, rightStage = page * 3 + n;
    const bool leftSelected = n > 0 && leftStage == s.stage;
    const bool rightSelected = n < visibleStages && rightStage == s.stage;
    if (leftSelected || rightSelected) continue;
    const int x = stageLeft + stageWidth * n / visibleStages;
    screen.target().line({int16_t(x), int16_t(timeline.y + 12)}, {int16_t(x), int16_t(timeline.bottom() - 12)}, 1, ink);
  }
  body.y += timelineH + 12;
  body.height -= timelineH + 12;
  const int controlsHeight = wide ? 116 : 130;
  const int sideWidth = wide ? std::min(260, int(body.width) / 3) : 0;
  const auto controls =
      wide ? fui::makeRect(body.right() - sideWidth, body.bottom() - controlsHeight, sideWidth, controlsHeight)
           : fui::makeRect(body.x, body.bottom() - controlsHeight, body.width, controlsHeight);
  const auto timer = wide ? fui::makeRect(body.x, body.y, body.width - sideWidth - 16, body.height)
                          : fui::makeRect(body.x, body.y, body.width, controls.y - body.y - 12);
  const int summaryHeight = options.showAccumulation ? (model.choosingAccumulation || wide ? 108 : 92) : 0;
  const auto summary = wide ? fui::makeRect(controls.x, body.y + 4, controls.width, summaryHeight)
                            : fui::makeRect(timer.x, timer.bottom() - summaryHeight, timer.width, summaryHeight);
  const int noticeY = wide ? timer.bottom() - 42 : summary.y - 36;
  const auto clock = fui::makeRect(timer.x + 4, timer.y + (wide ? 30 : 60), timer.width - 8,
                                   wide ? timer.height - 80 : summary.y - timer.y - 100);
  std::snprintf(work.label, sizeof(work.label), tr(STR_POMO_SESSION), int(s.stage / 2 + 1));
  if (wide) {
    dashboard::text(screen, fui::makeRect(timer.x + 8, timer.y, timer.width / 2 - 16, 30), phaseName(s.phase), 1);
    dashboard::text(screen, fui::makeRect(timer.x + timer.width / 2, timer.y, timer.width / 2 - 8, 30), work.label, 3);
  } else {
    dashboard::text(screen, fui::makeRect(timer.x + 8, timer.y, timer.width - 16, 32), phaseName(s.phase), 1);
    dashboard::text(screen, fui::makeRect(timer.x + 8, timer.y + 32, timer.width - 16, 26), work.label, 3);
  }
  const uint32_t seconds = (s.remainingMs + 999) / 1000;
  std::snprintf(work.label, sizeof(work.label), "%02lu:%02lu", static_cast<unsigned long>(seconds / 60),
                static_cast<unsigned long>(seconds % 60));
  drawCountdown(screen.target(), clock, work.label, 160);
  dashboard::text(screen, fui::makeRect(timer.x + 8, noticeY, timer.width - 16, 28),
                  s.notice    ? tr(STR_UI_TIME_FOR_A_NEW_SESSION_305DE9)
                  : s.running ? tr(STR_UI_IN_PROGRESS_BF8144)
                              : tr(STR_UI_READY_WHEN_YOU_ARE_31EF54),
                  3);
  if (options.showAccumulation) accumulationCard(screen, summary, model, work);
  const int startHeight = controlsHeight - 66, half = (controls.width - 12) / 2;
  timerui::control(screen, fui::makeRect(controls.x, controls.y, controls.width, startHeight),
                   s.running ? tr(STR_UI_PAUSE_9935DF) : tr(STR_UI_START_7196E7), Toggle, 0, true);
  timerui::control(screen, fui::makeRect(controls.x, controls.y + startHeight + 12, half, 54), tr(STR_UI_SKIP_431DE8),
                   Skip);
  timerui::control(screen,
                   fui::makeRect(controls.x + half + 12, controls.y + startHeight + 12, controls.width - half - 12, 54),
                   tr(STR_UI_RESTART_69ED00), Reset);
}
}  // namespace pomodoroui
