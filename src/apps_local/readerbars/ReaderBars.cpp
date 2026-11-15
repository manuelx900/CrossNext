#include "ReaderBars.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalPowerManager.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace readerbars {
namespace {
void duration(char* out, size_t size, uint32_t seconds) {
  if (seconds == UNKNOWN_TIME)
    std::snprintf(out, size, "--");
  else
    std::snprintf(out, size, tr(STR_READER_BAR_MINUTES), static_cast<unsigned long>((uint64_t(seconds) + 59) / 60));
}
int fit(GfxRenderer& renderer, char* text, int width, int measured) {
  if (width <= 0) {
    text[0] = 0;
    return 0;
  }
  if (measured <= width) return measured;
  const int dotsWidth = renderer.getTextWidth(SMALL_FONT_ID, "...");
  if (width < dotsWidth) {
    text[0] = 0;
    return 0;
  }
  width -= dotsWidth;
  int low = 0, high = std::min(60, static_cast<int>(std::strlen(text))), best = 0, bestWidth = 0;
  // Binary search a bounded UTF-8 prefix: at most seven extra measurements.
  while (low <= high) {
    const int mid = low + (high - low) / 2;
    int end = mid;
    while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    const char saved = text[end];
    text[end] = 0;
    const int candidate = renderer.getTextWidth(SMALL_FONT_ID, text);
    text[end] = saved;
    if (candidate <= width) {
      best = end;
      bestWidth = candidate;
      low = mid + 1;
    } else
      high = mid - 1;
  }
  std::memcpy(text + best, "...", 4);
  return bestWidth + dotsWidth;
}
void populate(GfxRenderer& renderer, const Config& config, const Data& data, Workspace& work) {
  for (unsigned i = 0; i < SLOT_COUNT; ++i) {
    auto& text = work.text[i];
    text[0] = 0;
    switch (config.slots[i]) {
      case Item::Empty:
        break;
      case Item::Clock:
        if (!halClock.formatTime(text, sizeof(text), SETTINGS.clockFormat == 1))
          std::snprintf(text, sizeof(text), "--");
        break;
      case Item::Battery:
        std::snprintf(text, sizeof(text), "%u%%", powerManager.getBatteryPercentage());
        break;
      case Item::TimeBook:
        duration(text, sizeof(text), data.timeBook);
        break;
      case Item::TimeChapter:
        duration(text, sizeof(text), data.timeChapter);
        break;
      case Item::Pages:
        std::snprintf(text, sizeof(text), "%s%d/%d", data.estimatedPages ? "~" : "", data.page, data.pages);
        break;
      case Item::Percent:
        std::snprintf(text, sizeof(text), "%.0f%%", data.percent);
        break;
      case Item::Book:
        copyLabel(text, data.book);
        break;
      case Item::Chapter:
        copyLabel(text, data.chapter);
        break;
      case Item::Count:
        break;
    }
    work.measured[i] = work.widths[i] = text[0] ? renderer.getTextWidth(SMALL_FONT_ID, text) : 0;
  }
}
}  // namespace

void copyLabel(char (&out)[64], const char* source) {
  const int length = std::snprintf(out, sizeof(out), "%s", source);
  if (length < static_cast<int>(sizeof(out))) return;
  size_t end = sizeof(out) - 4;
  while (end && (static_cast<unsigned char>(out[end]) & 0xc0) == 0x80) --end;
  std::memcpy(out + end, "...", 4);
}

int barHeight(const Config& config) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return height(config, metrics.statusBarVerticalMargin, metrics.progressBarMarginTop);
}

void draw(GfxRenderer& renderer, const Config& config, Position position, const Data& data, Workspace& work,
          int padding) {
  const int totalHeight = barHeight(config);
  if (!totalHeight) return;
  const auto& metrics = UITheme::getInstance().getMetrics();
  int top, right, bottom, left;
  renderer.getOrientedViewableTRBL(&top, &right, &bottom, &left);
  const int y =
      position == Position::Top ? top + padding : renderer.getScreenHeight() - bottom - totalHeight - padding - 4;
  left += metrics.statusBarHorizontalMargin;
  right = renderer.getScreenWidth() - right - metrics.statusBarHorizontalMargin;
  const int fullWidth = std::max(0, right - left);
  const int mark = data.bookmarked && config.hasText() ? 12 : 0;
  if (data.bookmarked && config.hasText()) {
    renderer.fillRect(left, y + 4, 6, 10, true);
    left += mark;
  }
  if (config.hasText()) {
    populate(renderer, config, data, work);
    constexpr int gap = 8;
    fitWidths(work.widths, right - left, gap);
    int leftX = left, rightWidth = 0;
    for (unsigned i = 4; i < SLOT_COUNT; ++i)
      if (work.widths[i]) rightWidth += work.widths[i] + gap;
    if (rightWidth) rightWidth -= gap;
    for (unsigned i = 0; i < 3; ++i) {
      if (!work.widths[i]) continue;
      fit(renderer, work.text[i], work.widths[i], work.measured[i]);
      renderer.drawText(SMALL_FONT_ID, leftX, y + 2, work.text[i]);
      leftX += work.widths[i] + gap;
    }
    int rightX = right - rightWidth;
    const int centreWidth = std::max(0, rightX - (rightWidth ? gap : 0) - leftX);
    if (work.widths[3]) {
      const int width = fit(renderer, work.text[3], centreWidth, work.measured[3]);
      renderer.drawText(SMALL_FONT_ID, leftX + (centreWidth - width) / 2, y + 2, work.text[3]);
    }
    for (unsigned i = 4; i < SLOT_COUNT; ++i) {
      if (!work.widths[i]) continue;
      fit(renderer, work.text[i], work.widths[i], work.measured[i]);
      renderer.drawText(SMALL_FONT_ID, rightX, y + 2, work.text[i]);
      rightX += work.widths[i] + gap;
    }
  }
  const int bar = config.progressHeight();
  if (bar) {
    const float percent = config.progress == Progress::Book ? data.percent
                          : data.pages > 0                  ? data.page * 100.0f / data.pages
                                                            : 0;
    renderer.fillRect(left - mark, y + totalHeight - bar, fullWidth * std::clamp(percent, 0.0f, 100.0f) / 100, bar,
                      true);
  }
}
}  // namespace readerbars
