#include "HomeScreen.h"

#include <ChromeRegion.h>
#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/HomeIcons.h"
#include "../ui/PixelText.h"
#include "../ui/StatusBattery.h"
#include "../ui/ToyboxTokens.h"
#include "HomeLoadingPlaceholders.h"

namespace homeui {
namespace {
constexpr fui::FontId kSmall = 0, kBody = 1, kCarouselTitle = 4;
constexpr int kBorder = 2;
constexpr auto black = fui::Paint::solid(fui::Color::Black);
constexpr auto white = fui::Paint::solid(fui::Color::White);

void text(fui::DrawTarget& target, fui::Rect rect, const char* value, fui::FontId font, int lines = 1,
          fui::Color color = fui::Color::Black, fui::TextAlign align = fui::TextAlign::Left, bool compact = false,
          bool emphasis = false) {
  fui::TextStyle style;
  style.font = font;
  style.bold = emphasis;
  style.color = color;
  style.align = align;
  style.maxLines = static_cast<uint8_t>(std::max(1, std::min(lines, int(rect.height / target.lineHeight(font)))));
  if (compact && style.maxLines == 1 && rect.height < target.lineHeight(font))
    rect = toybox::inkCentred(target, rect, style);
  target.text(rect, value, style);
}

void icon(fui::DrawTarget& target, const fui::Rect rect, Icon kind, bool inverted) {
  fui::BitmapRef bitmap;
  switch (kind) {
    case Icon::Files:
      bitmap = fui::bitmapFromIcon(homeicons::Folder(rect.width));
      break;
    case Icon::Library:
      bitmap = fui::bitmapFromIcon(homeicons::Library(rect.width));
      break;
    case Icon::Stats:
      bitmap = fui::bitmapFromIcon(homeicons::Stats(rect.width));
      break;
    case Icon::Transfer:
      bitmap = fui::bitmapFromIcon(homeicons::Send(rect.width));
      break;
    case Icon::Settings:
      bitmap = fui::bitmapFromIcon(homeicons::Settings(rect.width));
      break;
    case Icon::Gear:
      bitmap = fui::bitmapFromIcon(homeicons::Gear(rect.width));
      break;
    case Icon::Games:
      bitmap = fui::bitmapFromIcon(homeicons::Games(rect.width));
      break;
    case Icon::Apps:
      bitmap = fui::bitmapFromIcon(homeicons::Apps(rect.width));
      break;
    case Icon::Plugins:
      bitmap = fui::bitmapFromIcon(homeicons::Plugins(rect.width));
      break;
  }
  target.bitmap(rect, bitmap, fui::BitmapMode::Contain, inverted ? white : black);
}
}  // namespace

void drawIcon(fui::DrawTarget& target, fui::Rect rect, Icon kind, bool inverted) { icon(target, rect, kind, inverted); }

bool copyLabel(char* out, const size_t size, const char* text) { return pixelui::copyNatural(out, size, text); }

void formatDate(char* out, size_t size, int year, int month, int day) {
  static constexpr UiLabel months[] = {
      trLabel(STR_UI_JAN_521AD2), trLabel(STR_UI_FEB_F719E9), trLabel(STR_UI_MAR_A8D9B8), trLabel(STR_UI_APR_72402E),
      trLabel(STR_UI_MAY_18AE53), trLabel(STR_UI_JUN_F998A6), trLabel(STR_UI_JUL_7DA032), trLabel(STR_UI_AUG_FE905B),
      trLabel(STR_UI_SEP_492C5F), trLabel(STR_UI_OCT_AA5816), trLabel(STR_UI_NOV_F62CE2), trLabel(STR_UI_DEC_CA3A45)};
  if (year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) {
    std::snprintf(out, size, "-- ---");
    return;
  }
  std::snprintf(out, size, "%02d %s", day, months[month - 1].get());
}

Layout layout(const fui::DeviceContext& device, int count, Style style, const Model* model) {
  Layout out;
  out.style = style;
  out.count = std::max(1, std::min(count, kMaxRows));
  const auto safe = device.safeRect();
  if (style == Style::Boox) {
    const bool wide = device.width > device.height;
    out.header = fui::makeRect(0, 0, device.width, safe.y + 44);
    const auto body =
        fui::makeRect(safe.x + 12, out.header.bottom() + 12, safe.width - 24, safe.bottom() - out.header.bottom() - 20);
    out.settingsButton = fui::makeRect(safe.x + safe.width / 2 - 24, safe.y, 48, 44);
    out.menu = wide ? fui::makeRect(body.x + body.width * 65 / 100, body.y + body.height / 2, body.width * 35 / 100,
                                    body.height - body.height / 2)
                    : fui::makeRect(body.x, body.bottom() - 146, body.width, 146);
    out.card = wide ? fui::makeRect(body.x, body.y, out.menu.x - body.x - 16, body.height)
                    : fui::makeRect(body.x, body.y, body.width, out.menu.y - body.y - 12);
    const auto copy = wide ? fui::makeRect(out.menu.x, body.y, out.menu.width, out.menu.y - body.y - 10)
                           : fui::makeRect(body.x, out.card.bottom() - 132, body.width, 112);
    out.title = fui::makeRect(copy.x, copy.y, copy.width, 46);
    out.author = fui::makeRect(copy.x, out.title.bottom() + 2, copy.width, 26);
    out.progress = fui::makeRect(copy.x, out.author.bottom() + 2, copy.width, 36);
    out.bookProgress = fui::makeRect(wide ? out.menu.x : 0, out.menu.y - 17, wide ? out.menu.width : device.width, 4);
    out.coverRegion = wide ? out.card : fui::makeRect(0, out.card.y, device.width, copy.y - out.card.y - 8);
    const int height = out.coverRegion.height;
    const int width = std::min(out.coverRegion.width * 60 / 100, height * 2 / 3);
    out.cover =
        fui::makeRect(out.coverRegion.x + (out.coverRegion.width - width) / 2, out.coverRegion.y, width, height);
    const int sideWidth = std::max(0, (out.coverRegion.width - width) / 2 - 20);
    out.sides[1] = fui::makeRect(out.coverRegion.x, out.cover.y + height / 8, sideWidth, height * 3 / 4);
    out.sides[2] =
        fui::makeRect(out.coverRegion.right() - sideWidth, out.cover.y + height / 8, sideWidth, height * 3 / 4);
    const int primaryCount =
        model ? std::clamp(model->count - (settingsRow(*model) >= 0 ? 1 : 0), 0, 6) : std::min(out.count, 6);
    const int columns = primaryCount == 4 ? 2 : 3;
    const int rows = (primaryCount + columns - 1) / columns;
    const int cellHeight = out.menu.height / 2, cellWidth = out.menu.width / 3;
    const int groupTop = out.menu.y + (out.menu.height - rows * cellHeight) / 2;
    for (int n = 0; n < primaryCount; ++n) {
      const int row = n / columns;
      const int rowItems = std::min(columns, primaryCount - row * columns);
      const int col = n % columns;
      const int width = cellWidth;
      const int groupLeft = out.menu.x + (out.menu.width - rowItems * width) / 2;
      out.primary[n] = fui::makeRect(groupLeft + col * width, groupTop + row * cellHeight, width, cellHeight);
    }
    return out;
  }
  out.header = fui::makeRect(0, 0, device.width, safe.y + 72);
  const auto body = fui::makeRect(safe.x + 8, out.header.bottom() + 12, safe.width - 16,
                                  safe.bottom() - 8 - out.header.bottom() - 12);
  const bool wide = device.width > device.height;
  if (wide) {
    const int width = body.width * 43 / 100;
    out.card = fui::makeRect(body.x, body.y, width, body.height);
    out.menu = fui::makeRect(out.card.right() + 12, body.y, body.width - width - 12, body.height);
  } else {
    int cardHeight = std::min(248, std::max(144, int(body.height) - 6 * 60 - 12));
    cardHeight = std::min(cardHeight, body.height - 6 * 44 - 12);
    out.card = fui::makeRect(body.x, body.y, body.width, cardHeight);
    out.menu = fui::makeRect(body.x, out.card.bottom() + 12, body.width, body.bottom() - out.card.bottom() - 12);
  }
  const auto inner = out.card.inset(fui::Insets{12, 12, 12, 12});
  if (wide) {
    const int coverHeight = std::max(64, int(inner.height) - 110);
    const int coverWidth = coverHeight * 2 / 3;
    out.badge = fui::makeRect(inner.x, inner.y, inner.width, 44);
    out.cover =
        fui::makeRect(inner.x + (inner.width - coverWidth) / 2, out.badge.bottom() + 10, coverWidth, coverHeight);
    out.author = fui::makeRect(inner.x, inner.bottom() - 24, inner.width, 24);
    out.title = fui::makeRect(inner.x, out.cover.bottom() + 6, inner.width, out.author.y - out.cover.bottom() - 10);
  } else {
    const int coverWidth = std::min(int(inner.width * 35 / 100), inner.height * 2 / 3);
    out.cover = fui::makeRect(inner.x, inner.y, coverWidth, inner.height);
    const int x = out.cover.right() + 16, width = inner.right() - x;
    out.badge = fui::makeRect(x, inner.y + 4, width, 28);
    out.author = fui::makeRect(x, inner.bottom() - 36, width, 36);
    out.title = fui::makeRect(x, out.badge.bottom() + 10, width, out.author.y - out.badge.bottom() - 14);
  }
  out.previous = fui::makeRect(out.badge.right() - 92, out.badge.y, 44, 44);
  out.next = fui::makeRect(out.badge.right() - 44, out.badge.y, 44, 44);
  out.badge.width -= 100;
  out.badge.height = 44;
  if (!wide) {
    out.title.y += 16;
    out.title.height -= 16;
  }
  if (style != Style::Framed) {
    if (!wide) {
      const int menuHeight = 100;
      out.card = fui::makeRect(body.x, body.y, body.width, body.height - menuHeight - 12);
      out.menu = fui::makeRect(body.x, out.card.bottom() + 12, body.width, menuHeight);
    } else {
      const int menuWidth = 164;
      out.card = fui::makeRect(body.x, body.y, body.width - menuWidth - 12, body.height);
      out.menu = fui::makeRect(out.card.right() + 12, body.y, menuWidth, body.height);
    }
    const auto card = out.card.inset(fui::Insets{10, 10, 10, 10});
    out.badge = fui::makeRect(card.x, card.y, card.width - 100, 44);
    out.previous = fui::makeRect(card.right() - 92, card.y, 44, 44);
    out.next = fui::makeRect(card.right() - 44, card.y, 44, 44);
    const int footer = wide ? 92 : 134;
    const int top = wide ? 0 : 66;
    if (wide) out.badge = out.previous = out.next = {};
    const int artHeight = std::max(64, int(card.height) - top - footer);
    out.coverRegion = fui::makeRect(card.x, card.y + top, card.width, artHeight);
    const int coverWidth = std::min(card.width * 68 / 100, artHeight * 2 / 3);
    out.cover = fui::makeRect(card.x + (card.width - coverWidth) / 2, out.coverRegion.y, coverWidth, artHeight);
    const int nearW = std::max(18, std::min(coverWidth * 45 / 100, (card.width - coverWidth) / 2));
    const int farW = std::max(16, nearW * 75 / 100);
    out.sides[0] = fui::makeRect(std::max(int(card.x), int(out.cover.x - nearW - farW + 16)),
                                 out.cover.y + artHeight / 8, farW, artHeight * 3 / 4);
    out.sides[1] = fui::makeRect(out.cover.x - nearW + 8, out.cover.y + artHeight / 20, nearW, artHeight * 9 / 10);
    out.sides[2] = fui::makeRect(out.cover.right() - 8, out.cover.y + artHeight / 20, nearW, artHeight * 9 / 10);
    out.sides[3] = fui::makeRect(std::min(int(card.right() - farW), int(out.cover.right() + nearW - 16)),
                                 out.cover.y + artHeight / 8, farW, artHeight * 3 / 4);
    out.title = fui::makeRect(card.x + 4, out.cover.bottom() + (wide ? 20 : 18), card.width - 8, wide ? 22 : 48);
    out.author = fui::makeRect(card.x + 4, out.title.bottom() + 2, card.width - 8, wide ? 18 : 28);
    out.progress = fui::makeRect(card.x + 8, card.bottom() - (wide ? 22 : 26), card.width - 16, 24);
    out.bookProgress = fui::makeRect(out.cover.right() - std::min(58, int(out.cover.width) - 8) - 4, out.cover.y + 4,
                                     std::min(58, int(out.cover.width) - 8), 28);
  } else {
    out.coverRegion = out.cover;
    out.bookProgress = fui::makeRect(out.cover.right() - 72, out.cover.y + 4, 68, 28);
  }
  for (int n = 0; n < out.count; ++n) {
    const int top = out.menu.y + out.menu.height * n / std::max(6, out.count);
    const int bottom = out.menu.y + out.menu.height * (n + 1) / std::max(6, out.count);
    out.rows[n] = fui::makeRect(out.menu.x, top, out.menu.width, bottom - top);
    if (style == Style::Carousel) {
      if (!wide) {
        const int slots = std::max(6, out.count);
        const int groupLeft = out.menu.x + (out.menu.width - out.menu.width * out.count / slots) / 2;
        const int left = groupLeft + out.menu.width * n / slots;
        const int right = groupLeft + out.menu.width * (n + 1) / slots;
        out.rows[n] = fui::makeRect(left, out.menu.y, right - left, out.menu.height);
      } else {
        const int rowHeight = out.menu.height / ((std::max(6, out.count) + 1) / 2);
        const int usedRows = (out.count + 1) / 2;
        const int rowItems = std::min(2, out.count - n / 2 * 2);
        const int groupTop = out.menu.y + (out.menu.height - usedRows * rowHeight) / 2;
        const int groupLeft = out.menu.x + (out.menu.width - rowItems * (out.menu.width / 2)) / 2;
        out.rows[n] = fui::makeRect(groupLeft + n % 2 * (out.menu.width / 2), groupTop + n / 2 * rowHeight,
                                    out.menu.width / 2, rowHeight);
      }
    }
  }
  return out;
}

void drawMenuRow(fui::DrawTarget& target, const Model& model, const Layout& geometry, int n) {
  if (n < 0 || n >= geometry.count || n >= model.count) return;
  target.fill(geometry.rows[n], white);
  const auto row = geometry.rows[n];
  const bool selected = model.selected == n + (model.hasBook ? 1 : 0);
  if (model.style == Style::Carousel) {
    const auto art = fui::makeRect(row.x + (row.width - 40) / 2, row.y + 6, 40, 40);
    if (selected) target.fill(art.inset(fui::Insets{-6, -6, -6, -6}), black, 5);
    if (model.rows[n].shortcutIcon.data)
      target.bitmap(art, model.rows[n].shortcutIcon, fui::BitmapMode::Contain, selected ? white : black);
    else
      icon(target, art, model.rows[n].icon, selected);
    char brief[96];
    copyLabel(brief, sizeof(brief), model.rows[n].label);
    fui::TextStyle small;
    small.font = 3;
    small.bold = false;
    small.maxLines = 1;
    if (target.measureText(3, brief, small).width > row.width - 4) {
      if (model.rows[n].brief[0]) copyLabel(brief, sizeof(brief), model.rows[n].brief);
    }
    while (brief[0] && target.measureText(3, brief, small).width > row.width - 4) {
      const auto size = std::strlen(brief);
      if (size < 3) break;
      brief[size - 2] = '.';
      brief[size - 1] = 0;
    }
    text(target, fui::makeRect(row.x + 2, art.bottom() + 10, row.width - 4, 30), brief, 3, 1, fui::Color::Black,
         fui::TextAlign::Center, false, false);
    return;
  }
  if (selected) target.fill(row, black);
  const auto art = fui::makeRect(row.x + 20, row.y + (row.height - 40) / 2, 40, 40);
  if (model.rows[n].shortcutIcon.data)
    target.bitmap(art, model.rows[n].shortcutIcon, fui::BitmapMode::Contain, selected ? white : black);
  else
    icon(target, art, model.rows[n].icon, selected);
  const auto font = geometry.count == kMaxRows ? kCarouselTitle : kBody;
  text(target, fui::makeRect(row.x + 88, row.y + 4, row.width - 104, row.height - 8), model.rows[n].label, font, 1,
       selected ? fui::Color::White : fui::Color::Black, fui::TextAlign::Left, false, false);
  if (n > 0) target.fill(fui::makeRect(row.x, row.y, row.width, kBorder), black);
  if (n == geometry.count - 1) target.fill(fui::makeRect(row.x, row.bottom() - kBorder, row.width, kBorder), black);
}

void drawBoox(fui::DrawTarget& target, const Model& model, const Layout& g, bool lightHeader) {
  const auto ink = lightHeader ? black : white;
  target.fill(g.header, lightHeader ? white : black);
  const int top = g.header.bottom() - 44;
  text(target, fui::makeRect(16, top, g.header.width / 3, 44), model.time, kSmall, 1, ink.color);
  const auto battery = fui::makeRect(g.header.right() - 46, top + 12, statusbattery::width, statusbattery::height);
  statusbattery::draw(target, battery, model.battery, model.charging, ink.color);
  if (model.showPercent) {
    char value[8];
    std::snprintf(value, sizeof(value), "%d%%", model.battery);
    text(target, fui::makeRect(battery.x - 64, top, 58, 44), value, kSmall, 1, ink.color, fui::TextAlign::Right);
  }
  const int selected = model.selected - (model.hasBook ? 1 : 0);
  icon(target, g.settingsButton.inset({8, 8, 8, 8}), Icon::Gear, !lightHeader);
  if (selected == settingsRow(model)) target.stroke(g.settingsButton, ink, 2);
  if (model.hasBook) {
    text(target, g.title, model.title, 0, 1, fui::Color::Black, fui::TextAlign::Center);
    text(target, g.author, model.author, 3, 1, fui::Color::Black, fui::TextAlign::Center);
    char value[12];
    std::snprintf(value, sizeof(value), "%d%%", std::max(0, model.progress));
    text(target, g.progress, value, pixelui::uppercaseLabels ? 1 : 2, 1, fui::Color::Black, fui::TextAlign::Center);
  } else if (model.loadingShell) {
    loading::metadata(target, g.title, g.author, fui::TextAlign::Center);
    char value[8];
    std::snprintf(value, sizeof(value), "%d%%", 0);
    text(target, g.progress, value, pixelui::uppercaseLabels ? 1 : 2, 1, fui::Color::Black, fui::TextAlign::Center);
  } else {
    text(target, g.coverRegion, model.emptyHelp, kSmall, 3, fui::Color::Black, fui::TextAlign::Center);
  }
  target.fill(g.bookProgress, fui::Paint::dither(fui::Color::LightGray));
  const int filled = model.loadingShell ? 0 : g.bookProgress.width * std::clamp(model.progress, 0, 100) / 100;
  target.fill(fui::makeRect(g.bookProgress.x, g.bookProgress.y, filled, g.bookProgress.height), black);
  target.fill(fui::makeRect(g.bookProgress.x + std::clamp(filled - 3, 0, std::max(0, int(g.bookProgress.width) - 6)),
                            g.bookProgress.y - 2, 6, 8),
              black);
  for (int n = 0; n < 6; ++n) {
    const int index = primaryRow(model, n);
    if (index < 0) break;
    const auto& row = model.rows[index];
    const auto r = g.primary[n];
    const auto art = fui::makeRect(r.x + (r.width - 26) / 2, r.y + 8, 26, 26);
    const bool focus = selected == index;
    if (focus) target.fill(art.inset({-4, -4, -4, -4}), black, 6);
    if (row.shortcutIcon.data)
      target.bitmap(art, row.shortcutIcon, fui::BitmapMode::Contain, focus ? white : black);
    else
      icon(target, art, row.icon, focus);
    text(target, fui::makeRect(r.x + 3, r.y + 38, r.width - 6, std::max(20, int(r.height) - 40)), row.label,
         g.header.width > g.header.height * 12 ? 3 : 0, 2, fui::Color::Black, fui::TextAlign::Center);
  }
}

void draw(fui::DrawTarget& target, const Model& model, const Layout& geometry, bool outputInverted) {
  const bool lightHeader = chrome::headerIsLight();
  const bool paintLight = lightHeader != outputInverted;
  chrome::recordPaintedHeader(geometry.header.bottom(), paintLight);
  if (model.style == Style::Boox) {
    drawBoox(target, model, geometry, paintLight);
    return;
  }
  const auto ink = paintLight ? fui::Color::Black : fui::Color::White;
  target.fill(geometry.header, paintLight ? white : black);
  const int headerY = geometry.header.bottom() - 72;
  const int right = geometry.menu.right() - 12;
  const auto battery = statusbattery::headerIcon(right, headerY);
  statusbattery::draw(target, battery, model.battery, model.charging, ink);
  const int third = geometry.header.width / 3;
  text(target, fui::makeRect(geometry.card.x + 8, headerY + 4, third - 16, 64), model.date, kBody, 1, ink);
  text(target, fui::makeRect(third, headerY + 4, third, 64), model.time[0] ? model.time : "--:--", kBody, 1, ink,
       fui::TextAlign::Center);
  if (model.showPercent) {
    char label[8];
    std::snprintf(label, sizeof(label), "%d%%", std::clamp(model.battery, 0, 100));
    text(target, statusbattery::headerPercent(battery, headerY), label, kSmall, 1, ink, fui::TextAlign::Right);
  }
  if (model.style != Style::Carousel) target.stroke(geometry.card, black, kBorder);
  if (model.hasBook || model.loadingShell) {
    if (!geometry.badge.empty()) {
      target.fill(geometry.badge, lightHeader ? white : black);
      if (lightHeader) target.stroke(geometry.badge, black, kBorder);
      fui::TextStyle resume;
      resume.font = 3;
      resume.color = lightHeader ? fui::Color::Black : fui::Color::White;
      resume.maxLines = 1;
      const auto box = geometry.badge.inset(fui::Insets{0, 7, 0, 7});
      char label[80];
      pixelui::copyCaps(label, sizeof(label), model.continueLabel);
      if (target.measureText(resume.font, label, resume).width > box.width) {
        auto compact = resume;
        compact.font = kSmall;
        const auto originalInk = target.inkBounds(resume.font, "H", resume);
        const auto compactInk = target.inkBounds(compact.font, "H", compact);
        if (compactInk.height < originalInk.height && compactInk.height * 10 >= originalInk.height * 9 &&
            target.measureText(compact.font, label, compact).width <= box.width) {
          resume = compact;
        } else {
          pixelui::copyNatural(label, sizeof(label), model.continueLabel);
        }
      }
      target.text(toybox::inkCentred(target, box, resume), label, resume);
      for (int n = 0; n < 2; ++n) {
        const auto box = n ? geometry.next : geometry.previous;
        const auto ink = (n ? model.canNext : model.canPrevious) ? black : fui::Paint::solid(fui::Color::LightGray);
        target.stroke(box, ink, kBorder);
        const int cx = box.x + box.width / 2, cy = box.y + box.height / 2;
        const int direction = n ? 1 : -1;
        target.line({int16_t(cx - direction * 5), int16_t(cy - 10)}, {int16_t(cx + direction * 5), int16_t(cy)},
                    kBorder, ink);
        target.line({int16_t(cx + direction * 5), int16_t(cy)}, {int16_t(cx - direction * 5), int16_t(cy + 10)},
                    kBorder, ink);
      }
    }
    const bool hasAuthor = model.loadingShell || model.author[std::strspn(model.author, " \t\r\n")] != '\0';
    auto title = geometry.title;
    if (!hasAuthor) title.height = geometry.author.bottom() - title.y;
    const bool wide = geometry.menu.x > geometry.card.x;
    if (model.loadingShell) {
      loading::metadata(target, title, geometry.author,
                        model.style == Style::Framed && !wide ? fui::TextAlign::Left : fui::TextAlign::Center);
    } else {
      text(target, title, model.title,
           wide                             ? kSmall
           : model.style == Style::Carousel ? kCarouselTitle
                                            : kBody,
           hasAuthor ? (model.style == Style::Carousel ? 2 : 3) : 6, fui::Color::Black,
           model.style == Style::Framed && !wide ? fui::TextAlign::Left : fui::TextAlign::Center, wide);
    }
    if (model.style != Style::Framed) {
      const int dots = std::min(10, model.recentCount), unit = 14;
      const int left = geometry.coverRegion.x + (geometry.coverRegion.width - dots * unit) / 2;
      for (int n = 0; n < dots; ++n) {
        const auto dot = fui::makeRect(left + n * unit, geometry.cover.bottom() + 10, 8, 8);
        if (n == model.recentIndex)
          target.fill(dot, black);
        else
          target.stroke(dot, black, 1);
      }
      if (model.style == Style::Carousel) {
        const int progress = model.loadingShell ? 0 : std::clamp(model.progress, 0, 100);
        char percentage[8];
        std::snprintf(percentage, sizeof(percentage), "%d%%", progress);
        fui::TextStyle percentageStyle;
        percentageStyle.font = 3;
        const int labelWidth = target.measureText(3, percentage, percentageStyle).width;
        const int margin = std::min(24, int(geometry.progress.width) / 12);
        const auto bar = fui::makeRect(geometry.progress.x + margin, geometry.progress.y + 5,
                                       geometry.progress.width - 2 * margin - labelWidth - 6, 14);
        target.stroke(bar, black, 2);
        target.fill(fui::makeRect(bar.x + 2, bar.y + 2, (bar.width - 4) * progress / 100, bar.height - 4), black);
        text(target, fui::makeRect(bar.right() + 6, geometry.progress.y, labelWidth, 24), percentage, 3);
      }
    }
    if (hasAuthor && !model.loadingShell)
      text(target, geometry.author, model.author, wide ? fui::FontId{3} : kSmall, 1, fui::Color::Black,
           model.style == Style::Framed && !wide ? fui::TextAlign::Left : fui::TextAlign::Center, wide);
    target.stroke(geometry.cover, black, 1);
  } else if (!model.loadingShell) {
    const auto inner = geometry.card.inset(fui::Insets{20, 20, 20, 20});
    text(target, fui::makeRect(inner.x, inner.y, inner.width, inner.height / 2), model.emptyTitle, kBody, 2);
    text(target, fui::makeRect(inner.x, inner.y + inner.height / 2, inner.width, inner.height / 2), model.emptyHelp,
         kSmall, 2);
  }
  for (int n = 0; n < geometry.count && n < model.count; ++n) drawMenuRow(target, model, geometry, n);
  if (model.style != Style::Carousel) target.stroke(geometry.menu, black, kBorder);
}

void drawMenuSelection(fui::DrawTarget& target, Model& model, const Layout& geometry, int selected) {
  const int base = model.hasBook ? 1 : 0;
  const int previous = model.selected - base;
  model.selected = selected;
  drawMenuRow(target, model, geometry, previous);
  if (previous != selected - base) drawMenuRow(target, model, geometry, selected - base);
  if (model.style != Style::Carousel) target.stroke(geometry.menu, black, kBorder);
}

void drawCardSelection(fui::DrawTarget& target, const Model& model, const Layout& geometry) {
  if (model.style == Style::Boox) return;
  if (model.style == Style::Framed && model.hasBook && model.progress >= 0) {
    target.fill(geometry.bookProgress, white);
    target.stroke(geometry.bookProgress, black, 1);
    char percentage[8];
    std::snprintf(percentage, sizeof(percentage), "%d%%", std::clamp(model.progress, 0, 100));
    text(target, geometry.bookProgress, percentage, kSmall, 1, fui::Color::Black, fui::TextAlign::Center);
  }
  if (model.hasBook && model.selected == 0)
    target.stroke(model.style == Style::Carousel ? geometry.cover.inset(fui::Insets{-4, -4, -4, -4})
                                                 : geometry.card.inset(fui::Insets{6, 6, 6, 6}),
                  black, 2);
}
}  // namespace homeui
