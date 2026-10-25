#include "HomeScreen.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../components/icons/blocks.h"
#include "../../components/icons/folder.h"
#include "../../components/icons/settings2.h"
#include "../../components/icons/transfer.h"
#include "../ui/PixelText.h"
#include "../ui/ToyboxIcons.h"

namespace homeui {
namespace {
constexpr fui::FontId kSmall = 0, kBody = 1, kCarouselTitle = 4;
constexpr int kBorder = 4;
constexpr auto black = fui::Paint::solid(fui::Color::Black);
constexpr auto white = fui::Paint::solid(fui::Color::White);

void text(fui::DrawTarget& target, fui::Rect rect, const char* value, fui::FontId font, int lines = 1,
          fui::Color color = fui::Color::Black, fui::TextAlign align = fui::TextAlign::Left) {
  fui::TextStyle style;
  style.font = font;
  style.color = color;
  style.align = align;
  style.maxLines = static_cast<uint8_t>(std::max(1, std::min(lines, int(rect.height / target.lineHeight(font)))));
  target.text(rect, value, style);
}

void icon(fui::DrawTarget& target, const fui::Rect rect, Icon kind, bool inverted) {
  fui::BitmapRef bitmap;
  auto rotation = fui::Rotation::CW90;
  bitmap.width = bitmap.height = 32;
  bitmap.format = fui::BitmapFormat::Mask1;
  switch (kind) {
    case Icon::Files:
      bitmap.data = FolderIcon;
      break;
    case Icon::Library: {
      const auto ink = inverted ? white : black;
      const auto paper = inverted ? black : white;
      const auto book = fui::makeRect(rect.x + 6, rect.y + 2, rect.width - 12, rect.height - 4);
      target.stroke(book, ink, 3);
      target.fill(fui::makeRect(book.x + 2, book.bottom() - 9, book.width - 4, 3), ink);
      const auto ribbon = fui::makeRect(book.x + 10, book.y + 2, 9, 15);
      target.fill(ribbon, ink);
      target.triangle({ribbon.x, ribbon.bottom()}, {ribbon.right(), ribbon.bottom()},
                      {int16_t(ribbon.x + ribbon.width / 2), int16_t(ribbon.bottom() - 5)}, paper);
      return;
    }
    case Icon::Stats: {
      const auto ink = inverted ? white : black;
      target.stroke(rect.inset(fui::Insets{2, 2, 2, 2}), ink, 2);
      for (int n = 0; n < 3; ++n)
        target.fill(fui::makeRect(rect.x + 7 + n * 9, rect.bottom() - 8 - (n + 1) * 6, 6, (n + 1) * 6), ink);
      return;
    }
    case Icon::Transfer:
      bitmap.data = TransferIcon;
      break;
    case Icon::Settings:
      bitmap.data = Settings2Icon;
      break;
    case Icon::Plugins:
      bitmap.data = BlocksIcon;
      break;
    case Icon::Games:
      bitmap = fui::bitmapFromIcon(icon_games_32);
      rotation = fui::Rotation::None;
      break;
    case Icon::Apps:
      bitmap = fui::bitmapFromIcon(icon_apps_32);
      rotation = fui::Rotation::None;
      break;
  }
  target.bitmap(rect, bitmap, fui::BitmapMode::Contain, inverted ? white : black, rotation);
}
}  // namespace

void drawIcon(fui::DrawTarget& target, fui::Rect rect, Icon kind, bool inverted) { icon(target, rect, kind, inverted); }

bool copyLabel(char* out, const size_t size, const char* text) { return pixelui::copyCaps(out, size, text); }

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

Layout layout(const fui::DeviceContext& device, int count, Style style) {
  Layout out;
  out.style = style;
  out.count = std::max(1, std::min(count, kMaxRows));
  const auto safe = device.safeRect();
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
    const int coverHeight = std::min(176, inner.height / 2);
    const int coverWidth = coverHeight * 2 / 3;
    out.badge = fui::makeRect(inner.x, inner.y, inner.width, 28);
    out.cover =
        fui::makeRect(inner.x + (inner.width - coverWidth) / 2, out.badge.bottom() + 10, coverWidth, coverHeight);
    out.author = fui::makeRect(inner.x, inner.bottom() - 32, inner.width, 32);
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
  out.title.y += 16;
  out.title.height -= 16;
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
    const int extraTop = wide ? 12 : 28;
    const int top = 52 + extraTop, footer = 134;
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
    out.title = fui::makeRect(card.x + 4, out.cover.bottom() + 18, card.width - 8, 48);
    out.author = fui::makeRect(card.x + 4, out.title.bottom() + 2, card.width - 8, 28);
    out.progress = fui::makeRect(card.x + 8, card.bottom() - 26, card.width - 16, 24);
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
        const int left = out.menu.x + out.menu.width * n / std::max(6, out.count);
        const int right = out.menu.x + out.menu.width * (n + 1) / std::max(6, out.count);
        out.rows[n] = fui::makeRect(left, out.menu.y, right - left, out.menu.height);
      } else
        out.rows[n] = fui::makeRect(out.menu.x + n % 2 * out.menu.width / 2,
                                    out.menu.y + n / 2 * out.menu.height / ((std::max(6, out.count) + 1) / 2),
                                    out.menu.width / 2, out.menu.height / ((std::max(6, out.count) + 1) / 2));
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
         fui::TextAlign::Center);
    return;
  }
  if (selected) target.fill(row, black);
  const auto art = fui::makeRect(row.x + 20, row.y + (row.height - 40) / 2, 40, 40);
  if (model.rows[n].shortcutIcon.data)
    target.bitmap(art, model.rows[n].shortcutIcon, fui::BitmapMode::Contain, selected ? white : black);
  else
    icon(target, art, model.rows[n].icon, selected);
  text(target, fui::makeRect(row.x + 96, row.y + 4, row.width - 112, row.height - 8), model.rows[n].label, kBody, 1,
       selected ? fui::Color::White : fui::Color::Black);
  if (n > 0) target.fill(fui::makeRect(row.x, row.y, row.width, kBorder), black);
}

void draw(fui::DrawTarget& target, const Model& model, const Layout& geometry) {
  target.fill(geometry.header, black);
  const int headerY = geometry.header.bottom() - 72;
  const int right = geometry.menu.right() - 12;
  const auto battery = fui::makeRect(right - 34, headerY + 27, 30, 19);
  target.stroke(battery, white, 2);
  target.fill(fui::makeRect(battery.right(), battery.y + 5, 4, 9), white);
  const int percent = std::max(0, std::min(100, model.battery));
  const int fill = model.charging ? battery.width - 8 : (battery.width - 8) * percent / 100;
  target.fill(fui::makeRect(battery.x + 4, battery.y + 4, fill, battery.height - 8), white);
  if (model.charging) {
    const int x = battery.x + 13, y = battery.y - 1;
    target.line({int16_t(x + 5), int16_t(y)}, {int16_t(x), int16_t(y + 11)}, 2, black);
    target.line({int16_t(x), int16_t(y + 11)}, {int16_t(x + 7), int16_t(y + 9)}, 2, black);
    target.line({int16_t(x + 7), int16_t(y + 9)}, {int16_t(x + 2), int16_t(y + 20)}, 2, black);
  }
  const int third = geometry.header.width / 3;
  text(target, fui::makeRect(geometry.card.x + 8, headerY + 4, third - 16, 64), model.date, kBody, 1,
       fui::Color::White);
  text(target, fui::makeRect(third, headerY + 4, third, 64), model.time[0] ? model.time : "--:--", kBody, 1,
       fui::Color::White, fui::TextAlign::Center);
  if (model.showPercent) {
    char label[8];
    std::snprintf(label, sizeof(label), "%d%%", percent);
    text(target, fui::makeRect(battery.x - 70, headerY + 8, 66, 56), label, kSmall, 1, fui::Color::White);
  }
  if (model.style != Style::Carousel) target.stroke(geometry.card, black, kBorder);
  if (model.hasBook) {
    target.fill(geometry.badge, black);
    text(target, geometry.badge.inset(fui::Insets{0, 7, 0, 7}), model.continueLabel, 3, 1, fui::Color::White);
    for (int n = 0; n < 2; ++n) {
      const auto box = n ? geometry.next : geometry.previous;
      const auto ink = (n ? model.canNext : model.canPrevious) ? black : fui::Paint::solid(fui::Color::LightGray);
      target.stroke(box, ink, 3);
      const int cx = box.x + box.width / 2, cy = box.y + box.height / 2;
      const int direction = n ? 1 : -1;
      target.line({int16_t(cx - direction * 5), int16_t(cy - 10)}, {int16_t(cx + direction * 5), int16_t(cy)}, 4, ink);
      target.line({int16_t(cx + direction * 5), int16_t(cy)}, {int16_t(cx - direction * 5), int16_t(cy + 10)}, 4, ink);
    }
    const bool hasAuthor = model.author[std::strspn(model.author, " \t\r\n")] != '\0';
    auto title = geometry.title;
    if (!hasAuthor) title.height = geometry.author.bottom() - title.y;
    text(target, title, model.title, model.style == Style::Carousel ? kCarouselTitle : kBody,
         hasAuthor ? (model.style == Style::Carousel ? 2 : 3) : 6, fui::Color::Black,
         model.style == Style::Framed ? fui::TextAlign::Left : fui::TextAlign::Center);
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
        const int progress = std::clamp(model.progress, 0, 100);
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
    if (hasAuthor)
      text(target, geometry.author, model.author, kSmall, 1, fui::Color::Black,
           model.style == Style::Framed ? fui::TextAlign::Left : fui::TextAlign::Center);
    target.stroke(geometry.cover, black, 1);
  } else {
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
