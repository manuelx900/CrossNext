#include "QuotesScreens.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <cstdio>

#include "../library/CollectionScreens.h"
#include "../ui/CompactArt.h"
#include "../ui/PageIndicator.h"
#include "../ui/ReadingToolsUi.h"
#include "../ui/StatisticsArt.h"
#include "QuoteBookIcon.h"
#include "QuotesCore.h"

namespace quotesui {
namespace {
using fui::makeRect;
using namespace readingui;

constexpr int kControlHeight = 44;
constexpr int kRowInset = 10;
constexpr int kQuoteMarkHeight = 30;

void rule(toybox::Screen& s, fui::Rect area, int y) {
  s.target().line({int16_t(area.x), int16_t(y)}, {int16_t(area.right() - 1), int16_t(y)}, 1, dashboard::ink);
}

void labelCount(char* label, size_t size, int count, StrId singular, StrId plural) {
  std::snprintf(label, size, "%d %s", count, I18N.get(count == 1 ? singular : plural));
}

void orderControl(toybox::Screen& s, fui::Rect band, const char* label) {
  const auto icon = makeRect(band.right() - kControlHeight, band.y, kControlHeight, band.height);
  sort(s, icon.inset({4, 4, 4, 4}));
  text(s, makeRect(band.x + 8, band.y, icon.x - band.x - 8, band.height), label, 3, 1, fui::TextAlign::Right);
  dashboard::hit(s, band, Sort);
}

void cardText(fui::DrawTarget& target, fui::Rect box, const char* value, fui::FontId font, int lines = 1,
              fui::TextAlign align = fui::TextAlign::Left) {
  const int fittingLines = box.height / std::max(1, int(target.lineHeight(font)));
  if (fittingLines <= 0) return;
  fui::TextStyle style;
  style.font = font;
  style.maxLines = std::min(lines, fittingLines);
  style.align = align;
  target.text(box, value, style);
}

void tabs(toybox::Screen& s, fui::Rect band, int selected, bool inBook = false) {
  const char* labels[] = {tr(STR_RT_ALL), inBook ? tr(STR_RT_FAVORITES) : tr(STR_RT_BOOKS), tr(STR_RT_FAVORITES)};
  const int count = inBook ? 2 : 3;
  auto& target = s.target();
  for (int index = 0; index < count; ++index) {
    const auto cell = lightui::segmentRect(band, index, count);
    const bool active = index == selected;
    if (chrome::lightHeader) {
      lightui::segment(target, band, index, count, active);
    } else if (active) {
      target.fill(cell, dashboard::ink);
    } else if (index > 0 && selected != index - 1) {
      target.line({int16_t(cell.x), int16_t(band.y + 8)}, {int16_t(cell.x), int16_t(band.bottom() - 9)}, 1,
                  dashboard::ink);
    }
    text(s, cell.inset({4, 4, 4, 4}), labels[index], 0, 1, fui::TextAlign::Center, active);
    dashboard::hit(s, cell, inBook ? BookTab : Tab, index);
  }
  if (!chrome::lightHeader) dashboard::frame(s, band);
}

void button(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, bool primary = false) {
  lightui::surface(s.target(), r, primary ? dashboard::ink : dashboard::paper);
  dashboard::frame(s, r);
  const bool remove = action == ConfirmRemove;
  if (remove) compactart::draw(s, makeRect(r.x + 16, r.y + (r.height - 28) / 2, 28, 28), compactart::Trash);
  text(s, r.inset({4, 10, 4, int16_t(remove ? 54 : 10)}), label, 0, 1, fui::TextAlign::Center, primary);
  dashboard::hit(s, r, action);
}

int quotePositionWidth(int count) {
  if (count >= 100) return 106;
  return count >= 10 ? 76 : 52;
}

void detailActions(toybox::Screen& s, const DetailModel& m) {
  const auto head = pixelui::geometry(s.device()).header;
  const auto safe = s.device().safeRect();
  const int y = head.bottom() - (72 + kControlHeight) / 2;
  const auto more = makeRect(safe.right() - 10 - kControlHeight, y, kControlHeight, kControlHeight);
  const auto favorite = makeRect(more.x - kControlHeight - 8, y, kControlHeight, kControlHeight);
  const auto settings = makeRect(favorite.x - kControlHeight - 8, y, kControlHeight, kControlHeight);
  if (m.quoteCount > 1) {
    char position[24];
    std::snprintf(position, sizeof(position), "%d/%d", m.quoteIndex + 1, m.quoteCount);
    const int width = quotePositionWidth(m.quoteCount);
    pixelui::text(s.target(), makeRect(settings.x - 8 - width, y, width, kControlHeight), position, pixelui::kSmall,
                  fui::Color::White, fui::TextAlign::Right);
  }
  s.target().bitmap(settings.inset({8, 8, 8, 8}), fui::bitmapFromIcon(navicons::Gear32), fui::BitmapMode::Contain,
                    dashboard::paper);
  dashboard::hit(s, settings, TextSettings);
  star(s, favorite.inset({8, 8, 8, 8}), m.favorite, true);
  dashboard::hit(s, favorite, Favorite);
  const int cx = more.x + more.width / 2, cy = more.y + more.height / 2;
  for (int dx : {-9, 0, 9}) s.target().fill(makeRect(cx + dx - 2, cy - 2, 5, 5), dashboard::paper, 2);
  dashboard::hit(s, more, Options);
}

void iconLabel(toybox::Screen& s, fui::Rect band, const fui::BitmapRef& icon, const char* label, int font) {
  constexpr int iconSize = 24, gap = 7;
  statsart::draw(s, makeRect(band.x, band.y + (band.height - iconSize) / 2, iconSize, iconSize), icon);
  fui::TextStyle style;
  style.font = font;
  s.target().text(band.inset({0, 0, 0, iconSize + gap}), label, style);
}

int bookHeading(toybox::Screen& s, fui::Rect area, const char* title, const char* authorName, bool inQuote) {
  auto& target = s.target();
  const bool wide = s.device().width > s.device().height;
  constexpr int padding = 12;
  const int iconSize = inQuote ? 36 : 42;
  const int x = area.x + padding + iconSize + (inQuote ? 10 : 14);
  const int width = area.right() - x - padding;
  const int top = area.y + padding;
  int bottom = top;
  fui::TextStyle style;
  style.font = inQuote ? pixelui::kSmall : pixelui::kBody;
  style.bold = true;
  style.maxLines = (wide ? 1 : 2) + (inQuote ? 1 : 0);
  const auto titleArea = makeRect(x, top, width, 1);
  auto lineStyle = style;
  lineStyle.maxLines = 1;
  fui::layoutText(target, titleArea, title, style, [&](const char* line, fui::Rect run) {
    run.width = titleArea.right() - run.x;
    target.text(run, line, lineStyle);
    bottom = run.bottom();
  });
  if (*authorName) {
    fui::TextStyle author;
    author.font = inQuote ? 3 : pixelui::kSmall;
    const int height = target.lineHeight(author.font);
    target.text(makeRect(x, bottom + 3, width, height), authorName, author);
    bottom += height + 3;
  }
  bottom = std::max(bottom, top + iconSize);
  const auto& icon = inQuote ? statsart::openBook : kBookIcon;
  statsart::draw(s, makeRect(area.x + padding, top + (bottom - top - iconSize) / 2, iconSize, iconSize), icon);
  return bottom + padding;
}

int quoteMetadata(toybox::Screen& s, fui::Rect area, const DetailModel& m) {
  auto& target = s.target();
  int y = area.y;
  const bool hasDate = m.saved && *m.saved;
  constexpr int metadataFont = 3;
  const int height = hasDate || m.page > 0 ? std::max(38, int(target.lineHeight(metadataFont)) + 10) : 0;
  if (*m.title || *m.author) y = bookHeading(s, area, m.title, m.author, true);
  if (height) {
    if (y > area.y) rule(s, area, y);
    char label[144];
    const auto inner = area.inset({0, 10, 0, 10});
    const int dateWidth = m.page > 0 ? inner.width * 2 / 3 : inner.width;
    if (hasDate) {
      const char* date = m.saved;
      if (m.dateFormat && dateWidth >= 267) {
        std::snprintf(label, sizeof(label), m.dateFormat, m.saved);
        date = label;
      }
      iconLabel(s, makeRect(inner.x, y, dateWidth - (m.page > 0 ? 8 : 0), height), statsart::calendar, date,
                metadataFont);
    }
    if (m.page > 0) {
      int pageLeft = inner.x;
      if (hasDate) {
        const int divider = inner.x + dateWidth;
        target.line({int16_t(divider), int16_t(y + 7)}, {int16_t(divider), int16_t(y + height - 8)}, 1, dashboard::ink);
        pageLeft = divider + 9;
      }
      std::snprintf(label, sizeof(label), tr(STR_RT_PAGE), m.page);
      iconLabel(s, makeRect(pageLeft, y, inner.right() - pageLeft, height), statsart::page, label, metadataFont);
    }
    y += height;
  }
  if (y > area.y) dashboard::frame(s, makeRect(area.x, area.y, area.width, y - area.y));
  return y + 10;
}

void quoteMark(fui::DrawTarget& target, fui::Rect band, bool opening) {
  static constexpr uint8_t bits[] = {
      0b00000001, 0b10000000, 0b00011000, 0b00000111, 0b00000000, 0b01110000, 0b00001110, 0b00000000, 0b11100000,
      0b00011100, 0b00000001, 0b11000000, 0b00111000, 0b00000011, 0b10000000, 0b00111000, 0b00000011, 0b10000000,
      0b01110111, 0b00000111, 0b01110000, 0b01111111, 0b10000111, 0b11111000, 0b01111111, 0b11000111, 0b11111100,
      0b01111111, 0b11000111, 0b11111100, 0b01111111, 0b11000111, 0b11111100, 0b00111111, 0b10000011, 0b11111000,
      0b00011111, 0b00000001, 0b11110000, 0b00000000, 0b00000000, 0b00000000,
  };
  static constexpr fui::BitmapRef mark{bits, 24, 14, fui::BitmapFormat::BW1, false};
  constexpr int width = 36, height = 21;
  const int x = opening ? band.x : band.right() - width;
  const int y = band.y + (band.height - height) / 2;
  target.bitmap(makeRect(x, y, width, height), mark, fui::BitmapMode::Contain, dashboard::ink,
                opening ? fui::Rotation::None : fui::Rotation::R180);
}

void detailFooter(toybox::Screen& s, fui::Rect footer, const DetailModel& m, int bodyPage, int pages) {
  const int width = std::min(int(footer.width), 500);
  const auto band = makeRect(footer.x + (footer.width - width) / 2, footer.y, width, footer.height);
  const int sideWidth = m.quoteCount >= 100 ? 108 : 98;
  const int16_t pagerInset = m.quoteCount > 1 ? sideWidth + 5 : 0;
  if (pages > 1)
    pageui::build(s, band.inset({0, pagerInset, 0, pagerInset}), bodyPage + 1, pages, TextPage, -1, TextPage, 1);
  if (m.quoteCount < 2) return;
  const char* title = tr(STR_RT_QUOTE);
  fui::TextStyle style;
  style.font = pixelui::kSmall;
  const int naturalWidth = s.target().measureText(style.font, title, style).width;
  auto compactStyle = style;
  compactStyle.font = 3;
  int compactWidth = -1;
  for (int step : {-1, 1}) {
    const int next = m.quoteIndex + step;
    if (next < 0 || next >= m.quoteCount) continue;
    const bool previous = step < 0;
    const auto control = makeRect(previous ? band.x : band.right() - sideWidth, band.y, sideWidth, band.height);
    const auto glyph = makeRect(previous ? control.x : control.right() - 22, control.y, 22, control.height);
    constexpr int arrowGap = 3, labelGap = 5;
    const auto label = makeRect(previous ? glyph.right() + arrowGap : control.x, control.y,
                                control.width - glyph.width - arrowGap, control.height);
    dashboard::chevron(s.target(), glyph, false, previous);
    char destination[16];
    std::snprintf(destination, sizeof(destination), "%d", next + 1);
    int numberWidth = s.target().measureText(style.font, destination, style).width;
    const bool compact = naturalWidth + numberWidth + labelGap > label.width;
    const auto& labelStyle = compact ? compactStyle : style;
    if (compact) {
      if (compactWidth < 0) compactWidth = s.target().measureText(labelStyle.font, title, labelStyle).width;
      numberWidth = s.target().measureText(labelStyle.font, destination, labelStyle).width;
    }
    const int available = label.width - numberWidth - labelGap;
    const int titleWidth = std::min(compact ? compactWidth : naturalWidth, available);
    const int x = previous ? label.x : label.right() - titleWidth - labelGap - numberWidth;
    s.target().text(makeRect(x, label.y, titleWidth, label.height), title, labelStyle);
    s.target().text(makeRect(x + titleWidth + labelGap, label.y, numberWidth, label.height), destination, labelStyle);
    dashboard::hit(s, control, QuotePage, step);
  }
}

void alignmentMark(fui::DrawTarget& target, int x, int y, int value, fui::Paint color) {
  constexpr int width = 26;
  for (int i = 0; i < 4; ++i) {
    const int empty = i % 2 && value != int(pagedtext::Alignment::Justified) ? 10 : 0;
    const int left = x + (value == int(pagedtext::Alignment::Right)    ? empty
                          : value == int(pagedtext::Alignment::Center) ? empty / 2
                                                                       : 0);
    target.line({int16_t(left), int16_t(y + i * 6)}, {int16_t(left + width - empty), int16_t(y + i * 6)}, 2, color);
  }
}

void settingsAlignment(toybox::Screen& s, fui::Rect area, int selected) {
  constexpr int gap = 8, headingHeight = 44;
  const int rowHeight = std::max(52, int(s.target().lineHeight(pixelui::kSmall)) * 2 + 2);
  const char* labels[pagedtext::kAlignmentCount] = {tr(STR_ALIGN_LEFT), tr(STR_CENTER), tr(STR_ALIGN_RIGHT),
                                                    tr(STR_JUSTIFY)};
  fui::TextStyle title;
  title.font = pixelui::kSmall;
  title.bold = true;
  s.target().text(makeRect(area.x, area.y, area.width, headingHeight), tr(STR_ALIGNMENT), title);
  const int width = (area.width - gap) / 2;
  for (int i = 0; i < pagedtext::kAlignmentCount; ++i) {
    const auto choice = makeRect(area.x + (i % 2) * (width + gap), area.y + headingHeight + i / 2 * (rowHeight + gap),
                                 width, rowHeight);
    const bool active = i == selected;
    if (active)
      lightui::surface(s.target(), choice, dashboard::ink);
    else
      dashboard::frame(s, choice);
    if (s.frame().buttonSelected(SetAlignment, i))
      s.target().stroke(choice.inset({4, 4, 4, 4}), active ? dashboard::paper : dashboard::ink, 1, lightui::radius());
    alignmentMark(s.target(), choice.x + 12, choice.y + (choice.height - 18) / 2, i,
                  active ? dashboard::paper : dashboard::ink);
    fui::TextStyle label;
    label.font = pixelui::kSmall;
    label.align = fui::TextAlign::Center;
    label.maxLines = 2;
    label.color = active ? fui::Color::White : fui::Color::Black;
    s.target().text(choice.inset({1, 6, 1, 44}), labels[i], label);
    dashboard::hit(s, choice, SetAlignment, i);
  }
}

void settingPicker(toybox::Screen& s, fui::Rect row, StrId title, const char* value, fui::ActionId action) {
  constexpr int padding = 12, arrowWidth = 24;
  fui::TextStyle label;
  label.font = pixelui::kSmall;
  label.bold = true;
  fui::TextStyle current;
  current.font = pixelui::kSmall;
  current.maxLines = 2;
  const auto inner = row.inset({0, padding + arrowWidth, 0, padding});
  const int labelWidth = inner.width / 3;
  s.target().text(makeRect(inner.x, inner.y, labelWidth, inner.height), I18N.get(title), label);
  current.align = fui::TextAlign::Right;
  s.target().text(makeRect(inner.x + labelWidth + 4, inner.y, inner.width - labelWidth - 4, inner.height), value,
                  current);
  dashboard::chevron(s.target(), makeRect(row.right() - padding - arrowWidth, row.y, arrowWidth, row.height));
  dashboard::hit(s, row, action);
}

}  // namespace

int capacity(const fui::DeviceContext& d, bool books) {
  if (d.width > d.height) return 2;
  return books ? kMaxRows : 5;
}

void textSettings(toybox::Screen& s, const TextSettingsModel& model) {
  const auto body = begin(s, tr(STR_TEXT_SETTINGS)).inset({8, 10, 0, 10});
  const bool wide = s.device().width > s.device().height;
  constexpr int sectionGap = 18, footerHeight = 52, rowHeight = 68;
  const int columnWidth = wide ? (body.width - sectionGap) / 2 : body.width;
  char size[40];
  std::snprintf(size, sizeof(size), tr(STR_CARD_POINT_SIZE_FORMAT), unsigned(model.pointSize));
  const auto font = makeRect(body.x, body.y + 5, columnWidth, rowHeight);
  const auto sizeRow = makeRect(font.x, font.bottom(), font.width, font.height);
  dashboard::frame(s, makeRect(font.x, font.y, font.width, 2 * rowHeight));
  rule(s, font.inset({0, 12, 0, 12}), font.bottom());
  settingPicker(s, font, StrId::STR_FONT, model.font, PickFont);
  settingPicker(s, sizeRow, StrId::STR_SIZE, size, PickSize);
  const auto alignment = wide ? makeRect(body.right() - columnWidth, font.y, columnWidth, 0)
                              : makeRect(body.x, sizeRow.bottom() + sectionGap, body.width, 0);
  settingsAlignment(s, alignment, model.alignment);
  const auto footer = makeRect(body.x, body.bottom() - footerHeight, body.width, footerHeight);
  if (model.saveFailed)
    text(s, makeRect(footer.x, footer.y - 54, footer.width, 50), tr(STR_TOOLS_SAVE_FAILED), 0, 2,
         fui::TextAlign::Center);
  const int w = (footer.width - 12) / 2;
  dashboard::button(s, makeRect(footer.x, footer.y, w, footer.height), tr(STR_CANCEL), Back);
  dashboard::button(s, makeRect(footer.right() - w, footer.y, w, footer.height), tr(STR_GUIDE_SAVE), SaveTextSettings,
                    0, true);
}

const char* savedDateFormat() {
  // Shipped formats checked offline against Noto Sans Small and Pixel Tile with the widest date digits.
  // The minimum date column leaves 228 px of text width. Unreviewed packs display the date alone.
  constexpr uint64_t compactFormats[] = {
      0x31c03c2385a8dcbfull,  // Azerbaijani
      0x5ff17aa13f50c299ull,  // Catalan and Valencian
      0x6d6a0625072fd777ull,  // Czech
      0x6e06ac9c32185e92ull,  // Bosnian
      0x7a785f8b9fea9bf9ull,  // Bulgarian
      0x8c5c18d102333645ull,  // English
      0xa2f6c71ceeeeccc0ull,  // Norwegian
      0xa7284ea7a317da6eull,  // Danish
      0xbbfae8cf3954bf9full,  // Spanish
      0xd2eca59ec3740528ull,  // Belarusian
      0xe87493f6896387d8ull,  // Slovak
  };
  const char* format = tr(STR_RT_SAVED);
  return std::binary_search(std::begin(compactFormats), std::end(compactFormats), quotes::hash(format)) ? format
                                                                                                        : nullptr;
}

void list(toybox::Screen& s, const ListModel& m) {
  pixelui::begin(s);
  pixelui::Header header;
  header.title = m.inBook ? tr(STR_RT_BOOK_QUOTES) : tr(STR_QUOTES_APP);
  header.back = s.device().hasTouch;
  header.mark = fui::bitmapFromIcon(navicons::Gear32);
  header.markAction = TextSettings;
  pixelui::header(s, header);
  const auto b = pixelui::geometry(s.device()).body.inset({4, 4, 0, 4});
  const bool books = m.tab == 1 && !m.inBook;
  const bool favorites = m.inBook ? m.bookFavoritesOnly : m.tab == 2;
  const bool wide = s.device().width > s.device().height;
  char label[192];
  auto summary = makeRect(b.x, b.y + 52, b.width, kControlHeight);
  if (!m.inBook) tabs(s, makeRect(b.x, b.y, b.width, 46), m.tab);

  if (m.inBook) {
    const int headingBottom = bookHeading(s, b, m.title, m.author, false);
    dashboard::frame(s, makeRect(b.x, b.y, b.width, headingBottom - b.y));
    const int y = headingBottom + 12;
    const int tabWidth = wide ? b.width / 2 - 8 : b.width;
    tabs(s, makeRect(b.x, y, tabWidth, 46), favorites ? 1 : 0, true);
    const int x = wide ? b.x + tabWidth + 16 : b.x;
    summary = makeRect(x, y + (wide ? 0 : 52), b.right() - x, kControlHeight);
    if (favorites)
      labelCount(label, sizeof(label), m.bookFavorites, StrId::STR_RT_FAV_ONE, StrId::STR_RT_FAV_MANY);
    else
      labelCount(label, sizeof(label), m.bookTotal, StrId::STR_RT_QUOTE_ONE, StrId::STR_RT_QUOTE_MANY);
  } else if (favorites) {
    labelCount(label, sizeof(label), m.favorites, StrId::STR_RT_FAV_ONE, StrId::STR_RT_FAV_MANY);
  } else if (books) {
    labelCount(label, sizeof(label), m.books, StrId::STR_RT_BOOK_ONE, StrId::STR_RT_BOOK_MANY);
  } else {
    labelCount(label, sizeof(label), m.total, StrId::STR_RT_QUOTE_ONE, StrId::STR_RT_QUOTE_MANY);
  }

  const int summaryWidth = books ? summary.width : m.inBook && wide ? summary.width * 2 / 5 : summary.width / 2;
  int summaryX = summary.x + kRowInset;
  if (favorites) {
    constexpr int iconSize = 20;
    star(s, makeRect(summaryX, summary.y + (kControlHeight - iconSize) / 2, iconSize, iconSize), true);
    summaryX += iconSize + 10;
  }
  text(s, makeRect(summaryX, summary.y, summary.x + summaryWidth - summaryX, summary.height), label, 0);
  if (!books && m.visible) {
    const char* order = m.inBook && m.oldest ? tr(STR_RT_BOOK_ORDER) : m.oldest ? tr(STR_RT_OLDEST) : tr(STR_RT_RECENT);
    orderControl(s, makeRect(summary.x + summaryWidth, summary.y, summary.width - summaryWidth, summary.height), order);
  }
  const int top = summary.bottom() + 6;
  const bool oneBookPage = books && m.visible && m.pages == 1;
  const int listEnd = b.bottom() - (m.pages > 1 || oneBookPage ? kControlHeight + 8 : 0);
  const int cardAreaEnd = b.bottom() - kControlHeight - 8;
  const int rowHeight =
      std::min(books ? collectionui::ListPitch : 106, (cardAreaEnd - top) / capacity(s.device(), books));
  const int cardGap = books ? collectionui::ListPitch - collectionui::ListHeight : 8;

  for (int n = 0; n < m.visible; ++n) {
    const auto& row = m.rows[n];
    const auto r = makeRect(b.x, top + n * rowHeight, b.width, rowHeight - cardGap);
    dashboard::frame(s, r);
    if (books) {
      statsart::draw(s, makeRect(r.x + 12, r.y + (r.height - 32) / 2, 32, 32), kBookIcon);
      const int titleX = r.x + 56;
      const auto arrow = makeRect(r.right() - 28, r.y, 20, r.height);
      const int metaHeight = std::max(26, int(s.target().lineHeight(3)));
      const auto meta = makeRect(titleX, r.bottom() - metaHeight - 4, arrow.x - titleX - 8, metaHeight);
      cardText(s.target(), makeRect(titleX, r.y + 3, meta.width, meta.y - r.y - 3), row.title, pixelui::kSmall, 2);
      labelCount(label, sizeof(label), row.count, StrId::STR_RT_QUOTE_ONE, StrId::STR_RT_QUOTE_MANY);
      constexpr int countWidth = 81;
      cardText(s.target(), makeRect(meta.x, meta.y, meta.width - countWidth - 8, meta.height), row.author, 3);
      cardText(s.target(), makeRect(meta.right() - countWidth, meta.y, countWidth, meta.height), label, 3, 1,
               fui::TextAlign::Right);
      dashboard::chevron(s.target(), arrow);
      dashboard::hit(s, r, Open, row.index);
      continue;
    } else {
      const int textX = r.x + kRowInset;
      const auto arrow = makeRect(r.right() - 28, r.y, 20, r.height);
      const int textWidth = arrow.x - textX - 8;
      const int metaHeight = std::max(26, int(s.target().lineHeight(3)));
      const auto meta = makeRect(textX, r.bottom() - metaHeight - 5, textWidth, metaHeight);
      cardText(s.target(), makeRect(textX, r.y + 4, textWidth, meta.y - r.y - 7), row.preview, pixelui::kSmall, 2);
      if (m.inBook) {
        if (row.page > 0) {
          std::snprintf(label, sizeof(label), tr(STR_RT_PAGE), row.page);
          iconLabel(s, meta, statsart::page, label, 3);
        }
      } else if (*row.title) {
        iconLabel(s, meta, statsart::openBook, row.title, 3);
      }
      dashboard::chevron(s.target(), arrow);
      dashboard::hit(s, r, Open, row.index);
    }
  }

  if (!m.visible) {
    const auto empty = makeRect(b.x + kRowInset, top + 8, b.width - 2 * kRowInset, listEnd - top - 8);
    text(s, empty, favorites ? tr(STR_RT_FAVORITES_EMPTY_OPEN_QUOTE) : tr(STR_QUOTES_HELP), 0, 6,
         fui::TextAlign::Center);
  }
  const auto footer = makeRect(b.x, b.bottom() - kControlHeight, b.width, kControlHeight);
  if (m.pages > 1)
    pageui::build(s, footer, m.page + 1, m.pages, Page, -1, Page, 1);
  else if (oneBookPage) {
    const auto count = pageui::layout(footer).count;
    dashboard::frame(s, count);
    pixelui::text(s.target(), count, "1 / 1", pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
  }
}

int detail(toybox::Screen& s, const DetailModel& m, pagedtext::Work& work) {
  pixelui::begin(s);
  pixelui::Header header;
  header.title = tr(STR_QUOTES_APP);
  header.clockText = s.headerStatus().clock;
  header.back = s.device().hasTouch;
  header.titleReserve = 3 * kControlHeight + 16 + (m.quoteCount > 1 ? quotePositionWidth(m.quoteCount) + 8 : 0);
  pixelui::header(s.target(), s.device(), header);
  const auto b = pixelui::geometry(s.device()).body.inset({4, 4, 0, 4});
  detailActions(s, m);

  const auto passage = b.inset({0, kRowInset, 0, kRowInset});
  const auto footer = makeRect(b.x, b.bottom() - kControlHeight, b.width, kControlHeight);
  const bool hasBody = *m.body;
  const int markHeight = hasBody ? kQuoteMarkHeight : 0;
  const int bodyTop = quoteMetadata(s, b, m);
  auto content = makeRect(passage.x, bodyTop + (m.bodyPage == 0 ? markHeight : 0), passage.width,
                          footer.y - bodyTop - 6 - 2 * markHeight);
  // Both quote marks and a whole text line must fit above the pager.
  const auto font = s.target().lineHeight(kReadingFont) <= content.height ? kReadingFont : pixelui::kSmall;
  int paintedBottom;
  const int pages = pagedtext::draw(s.target(), content, m.body, font, m.bodyPage, work, &paintedBottom, m.alignment,
                                    pagedtext::Source::InvalidateOnChange);
  const int bodyPage = std::min(m.bodyPage, pages - 1);
  if (bodyPage != m.bodyPage) {
    if (bodyPage == 0) content.y += markHeight;
    pagedtext::draw(s.target(), content, m.body, font, bodyPage, work, &paintedBottom, m.alignment,
                    pagedtext::Source::InvalidateOnChange);
  }
  if (hasBody && bodyPage == 0) quoteMark(s.target(), makeRect(passage.x, bodyTop, passage.width, markHeight), true);
  if (hasBody && bodyPage == pages - 1)
    quoteMark(s.target(), makeRect(passage.x, paintedBottom, passage.width, markHeight), false);
  detailFooter(s, footer, m, bodyPage, pages);
  return pages;
}

void message(toybox::Screen& s, const char* copy, bool confirm) {
  const auto b = begin(s, tr(STR_QUOTES_APP));
  const int footer = b.bottom() - 54;
  const int width = (b.width - 12) / 2;
  text(s, makeRect(b.x + 18, b.y + 12, b.width - 36, footer - b.y - 24), copy, 0, 6, fui::TextAlign::Center);
  if (confirm) {
    button(s, makeRect(b.x, footer, width, 54), tr(STR_RT_DELETE_QUOTE), ConfirmRemove);
    button(s, makeRect(b.right() - width, footer, width, 54), tr(STR_RT_KEEP_QUOTE), Cancel, true);
  } else {
    button(s, makeRect(b.x, footer, b.width, 54), tr(STR_BACK), Back);
  }
}
}  // namespace quotesui
