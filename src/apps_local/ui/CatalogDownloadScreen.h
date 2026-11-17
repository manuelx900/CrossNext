#pragma once
#include <FreeInkUI.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

namespace networkui {
namespace fui = freeink::ui;
inline void transferSize(char* out, size_t capacity, size_t bytes) {
  if (bytes < 1024)
    std::snprintf(out, capacity, "%u B", static_cast<unsigned>(bytes));
  else if (bytes < 1024 * 1024)
    std::snprintf(out, capacity, "%u.%u KB", static_cast<unsigned>(bytes / 1024),
                  static_cast<unsigned>((bytes % 1024) * 10 / 1024));
  else
    std::snprintf(out, capacity, "%u.%u MB", static_cast<unsigned>(bytes / (1024 * 1024)),
                  static_cast<unsigned>((bytes % (1024 * 1024)) * 10 / (1024 * 1024)));
}
template <typename Screen>
void download(Screen& screen, const char* status, const size_t progress, const size_t total,
              const fui::ActionId cancelAction, const int16_t pad) {
  // Centered block: status line, item title, progress, optional cancel.
  const auto& theme = screen.theme();
  fui::TextStyle centered = theme.smallText;
  centered.align = fui::TextAlign::Center;
  const int16_t lh = screen.target().lineHeight(centered.font);
  const int16_t gap = theme.spaceMd;
  const int16_t progressH = 18 + gap + lh;
  const bool withCancel = cancelAction != fui::NO_ACTION;
  const int16_t btnH = withCancel ? theme.rowHeight : 0;
  // The item title gets two padded lines: long book titles wrap once and then
  // truncate, instead of running edge-to-edge in a single clipped line.
  fui::TextStyle title = centered;
  title.maxLines = 2;
  const int16_t titleH = static_cast<int16_t>(lh * 2);
  const int16_t blockH = static_cast<int16_t>(lh + titleH + progressH + btnH + gap * (withCancel ? 3 : 2));
  const fui::Rect body = screen.body();
  if (body.height > blockH) screen.spacer(static_cast<int16_t>((body.height - blockH) / 2));

  screen.target().text(screen.takeTop(lh, gap), tr(STR_DOWNLOADING), centered);
  screen.target().text(screen.takeTop(titleH, gap).inset(fui::Insets{0, pad, 0, pad}), status, title);

  const fui::Rect progressArea = screen.takeTop(progressH, gap).inset(fui::Insets{0, 50, 0, 50});
  const auto bar = fui::makeRect(progressArea.x, progressArea.y, progressArea.width, 18);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  screen.target().stroke(bar, ink, 1);
  const int percent = total ? static_cast<int>(std::min<uint64_t>(100, uint64_t(progress) * 100 / total)) : 0;
  if (total) {
    screen.target().fill(fui::makeRect(bar.x + 2, bar.y + 2, (bar.width - 4) * percent / 100, bar.height - 4), ink);
  } else {
    // A striped band denotes an unknown length; it never implies a percentage.
    for (int x = bar.x + 3; x < bar.right() - 3; x += 8)
      screen.target().line({int16_t(x), int16_t(bar.y + 3)},
                           {int16_t(std::min(x + 6, int(bar.right()) - 3)), int16_t(bar.bottom() - 3)}, 1, ink);
  }
  char bytes[64], received[24], expected[24];
  transferSize(received, sizeof(received), progress);
  transferSize(expected, sizeof(expected), total);
  if (total)
    snprintf(bytes, sizeof(bytes), "%d%%  ·  %s / %s", percent, received, expected);
  else if (progress)
    snprintf(bytes, sizeof(bytes), "%s", received);
  else
    snprintf(bytes, sizeof(bytes), "%s", tr(STR_CONNECTING));
  screen.target().text(fui::makeRect(progressArea.x, bar.bottom() + gap, progressArea.width, lh), bytes, centered);

  if (withCancel) {
    const fui::Rect btnArea = screen.takeTop(btnH);
    const int16_t btnW = static_cast<int16_t>(btnArea.width / 3);
    fui::ButtonProps cancel;
    cancel.label = tr(STR_CANCEL);
    cancel.action = cancelAction;
    screen.button(cancel,
                  fui::Rect{static_cast<int16_t>(btnArea.x + (btnArea.width - btnW) / 2), btnArea.y, btnW, btnH});
  }
}

}  // namespace networkui
