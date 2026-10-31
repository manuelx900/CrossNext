#include "QuotesScreens.h"

#include <I18n.h>

#include <cstdio>

#include "../ui/CompactArt.h"
#include "../ui/ExtraIcons.h"
#include "../ui/PageIndicator.h"
#include "../ui/ReadingToolsUi.h"
namespace quotesui {
namespace {
using fui::makeRect;
using namespace readingui;
void button(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, bool primary = false) {
  lightui::surface(s.target(), r, primary ? dashboard::ink : dashboard::paper);
  dashboard::frame(s, r, 2);
  if (action == Favorite) star(s, makeRect(r.x + 16, r.y + (r.height - 36) / 2, 36, 36), true, primary);
  if (action == Remove || action == ConfirmRemove)
    compactart::draw(s, makeRect(r.x + 16, r.y + (r.height - 36) / 2, 36, 36), compactart::Trash);
  const bool icon = action == Favorite || action == Remove || action == ConfirmRemove;
  caps(s, makeRect(r.x + (icon ? 72 : 10), r.y + 6, r.width - (icon ? 82 : 20), r.height - 12), label, 1, 2,
       fui::TextAlign::Center, primary);
  dashboard::hit(s, r, action);
}
void pager(toybox::Screen& s, fui::Rect r, int current, int total) {
  pageui::build(s, r, current + 1, total, Page, -1, Page, 1);
}
}  // namespace
int capacity(const fui::DeviceContext& d) { return d.width > d.height ? 2 : 5; }
void list(toybox::Screen& s, const ListModel& m) {
  const auto b = begin(s, m.inBook ? tr(STR_RT_BOOK_QUOTES) : tr(STR_QUOTES_APP));
  char label[256];
  int top = b.y;
  if (m.inBook) {
    auto hero = makeRect(b.x, top, b.width, 106);
    dashboard::frame(s, hero, 2);
    book(s, makeRect(hero.x + 16, hero.y + 18, 54, 68));
    caps(s, makeRect(hero.x + 90, hero.y + 6, hero.width - 104, 36), m.title, 1);
    text(s, makeRect(hero.x + 90, hero.y + 44, hero.width - 104, 28), m.author);
    std::snprintf(label, sizeof(label), "%d %s · %d %s", m.bookTotal,
                  m.bookTotal == 1 ? tr(STR_RT_QUOTE_ONE) : tr(STR_RT_QUOTE_MANY), m.bookFavorites,
                  m.bookFavorites == 1 ? tr(STR_RT_FAV_ONE) : tr(STR_RT_FAV_MANY));
    caps(s, makeRect(hero.x + 90, hero.y + 76, hero.width - 104, 26), label, 3);
    top += 116;
  } else {
    std::snprintf(label, sizeof(label), "%d %s · %d %s", m.total,
                  m.total == 1 ? tr(STR_RT_QUOTE_ONE) : tr(STR_RT_QUOTE_MANY), m.books,
                  m.books == 1 ? tr(STR_RT_BOOK_ONE) : tr(STR_RT_BOOK_MANY));
    caps(s, makeRect(b.x, top, b.width, 32), label, 0);
    top += 38;
    const char* titles[] = {tr(STR_RT_ALL), tr(STR_RT_BOOKS), tr(STR_RT_FAVORITES)};
    for (int n = 0; n < 3; ++n) {
      auto r = makeRect(b.x + n * b.width / 3, top, b.width * (n + 1) / 3 - n * b.width / 3, 48);
      if (chrome::lightHeader)
        lightui::segment(s.target(), makeRect(b.x, top, b.width, 48), n, 3, n == m.tab);
      else {
        s.target().fill(r, n == m.tab ? dashboard::ink : dashboard::paper);
        dashboard::frame(s, r, 2);
      }
      caps(s, r.inset(fui::Insets{4, 3, 4, 3}), titles[n], 0, 1, fui::TextAlign::Center, n == m.tab);
      dashboard::hit(s, r, Tab, n);
    }
    top += 58;
  }
  if (m.tab != 1 || m.inBook) {
    auto r = makeRect(b.x, top, b.width, 44);
    const char* order = m.inBook && m.oldest ? tr(STR_RT_BOOK_ORDER) : m.oldest ? tr(STR_RT_OLDEST) : tr(STR_RT_RECENT);
    if (m.tab == 2 && !m.inBook) {
      std::snprintf(label, sizeof(label), "%d %s", m.favorites,
                    m.favorites == 1 ? tr(STR_RT_FAV_ONE) : tr(STR_RT_FAV_MANY));
      caps(s, makeRect(r.x, r.y, r.width / 3, r.height), label, 3);
      caps(s, makeRect(r.x + r.width / 3, r.y, r.width * 2 / 3 - 50, r.height), order, 3, 1, fui::TextAlign::Right);
    } else
      caps(s, makeRect(r.x + 4, r.y, r.width - 56, r.height), order, 0);
    sort(s, makeRect(r.right() - 44, r.y, 44, 44));
    dashboard::hit(s, r, Sort);
    top += 48;
  }
  const int cap = capacity(s.device()), gap = 4, h = (b.bottom() - 46 - top - gap * (cap - 1)) / cap;
  for (int n = 0; n < std::min(m.visible, cap); ++n) {
    const auto& row = m.rows[n];
    auto r = makeRect(b.x, top + n * (h + gap), b.width, h);
    dashboard::frame(s, r, 2);
    if (m.tab == 1 && !m.inBook) {
      book(s, makeRect(r.x + 14, r.y + (r.height - 52) / 2, 46, 52));
      caps(s, makeRect(r.x + 80, r.y + 8, r.width - 188, 32), row.title, 0);
      text(s, makeRect(r.x + 80, r.y + 44, r.width - 188, 30), row.author, 4);
      s.target().line({int16_t(r.right() - 102), int16_t(r.y + 12)},
                      {int16_t(r.right() - 102), int16_t(r.bottom() - 12)}, 1, dashboard::ink);
      std::snprintf(label, sizeof(label), "%d", row.count);
      caps(s, makeRect(r.right() - 94, r.y + 8, 64, 32), label, 1, 1, fui::TextAlign::Center);
      caps(s, makeRect(r.right() - 94, r.y + 44, 64, 24), row.count == 1 ? tr(STR_RT_QUOTE_ONE) : tr(STR_QUOTES_APP), 3,
           1, fui::TextAlign::Center);
      dashboard::chevron(s.target(), makeRect(r.right() - 28, r.y, 22, r.height));
      dashboard::hit(s, r, Open, row.index);
    } else {
      text(s, makeRect(r.x + 10, r.y + 6, r.width - 84, std::max(28, h - 36)), row.preview, 4, 2);
      if (m.inBook)
        std::snprintf(label, sizeof(label), tr(STR_RT_PAGE), row.page);
      else
        std::snprintf(label, sizeof(label), "%s · %s", row.title, row.author);
      text(s, makeRect(r.x + 10, r.bottom() - 30, r.width - 80, 28), label, 4, 1, fui::TextAlign::Left, false,
           !m.inBook);
      const auto fav = makeRect(r.right() - 54, r.y + 3, 48, 44);
      star(s, fav.inset(fui::Insets{6, 6, 6, 6}), row.favorite);
      dashboard::hit(s, fav, Favorite, row.index);
      dashboard::hit(s, makeRect(r.x, r.y, r.width - 60, r.height), Open, row.index);
      const auto arrow = makeRect(r.right() - 54, r.bottom() - 45, 48, 44);
      if (arrow.y >= fav.bottom()) {
        dashboard::chevron(s.target(), arrow);
        dashboard::hit(s, arrow, Open, row.index);
      }
    }
  }
  if (!m.visible) {
    const auto notice = makeRect(b.x + 12, top + 20, b.width - 24, std::max(0, b.bottom() - 60 - top - 20));
    const int lines = std::max(1, int(notice.height) / std::max(1, int(s.target().lineHeight(4))));
    text(s, notice, m.tab == 2 ? tr(STR_RT_NO_FAVORITES) : tr(STR_QUOTES_HELP), 4, lines);
  }
  pager(s, makeRect(b.x, b.bottom() - 44, b.width, 44), m.page, m.pages);
}
int detail(toybox::Screen& s, const DetailModel& m, pagedtext::Work& work) {
  auto b = begin(s, tr(STR_RT_QUOTE));
  const auto head = pixelui::geometry(s.device()).header;
  const auto favorite = makeRect(head.right() - 64, head.y + (head.height - 48) / 2, 48, 48);
  s.target().fill(makeRect(head.right() - 180, head.y, 180, head.height), dashboard::ink);
  star(s, favorite.inset(fui::Insets{6, 6, 6, 6}), m.favorite, true);
  dashboard::hit(s, favorite, Favorite, m.index);
  const bool wide = b.width > b.height;
  const int mainW = wide ? b.width * 3 / 5 : b.width;
  auto meta = makeRect(b.x, b.y, b.width, 54);
  dashboard::frame(s, meta, 2);
  caps(s, makeRect(meta.x + 8, meta.y + 4, meta.width / 2 - 16, meta.height - 8), m.title, 0, 2);
  text(s, makeRect(meta.x + meta.width / 2, meta.y + 4, meta.width / 3 - 8, meta.height - 8), m.author, 4, 2);
  char label[128];
  std::snprintf(label, sizeof(label), tr(STR_RT_PAGE), m.page);
  caps(s, makeRect(meta.right() - 72, meta.y + 4, 64, meta.height - 8), label, 3, 2, fui::TextAlign::Center);
  auto content = makeRect(b.x + 10, meta.bottom() + 24, mainW - 20,
                          (wide ? b.bottom() - 88 : b.bottom() - 286) - meta.bottom() - 24);
  const int pages = pagedtext::draw(s.target(), content, m.body, kReadingFont, m.bodyPage, work);
  const int bodyPage = std::clamp(m.bodyPage, 0, pages - 1);
  if (bodyPage != m.bodyPage) pagedtext::draw(s.target(), content, m.body, kReadingFont, bodyPage, work);
  const int ax = wide ? b.x + mainW + 16 : b.x, aw = wide ? b.width - mainW - 16 : b.width;
  const int ay = wide ? meta.bottom() + 22 : b.bottom() - 246;
  if (!wide)
    s.target().line({int16_t(b.x + 8), int16_t(ay - 10)}, {int16_t(b.right() - 8), int16_t(ay - 10)}, 1,
                    dashboard::ink);
  if (m.saved && *m.saved) {
    std::snprintf(label, sizeof(label), tr(STR_RT_SAVED), m.saved);
    text(s, makeRect(ax + 8, ay, aw - 16, 44), label, 4, 2);
  }
  const int by = ay + 54;
  button(s, makeRect(ax, by, aw, 64), m.favorite ? tr(STR_RT_UNFAVORITE) : tr(STR_RT_FAVORITE), Favorite, true);
  button(s, makeRect(ax, by + 76, aw, 64), tr(STR_RT_DELETE_QUOTE), Remove);
  const auto controls = pageui::draw(s.target(), makeRect(b.x, b.bottom() - 44, mainW, 44), m.index + 1, m.total);
  if (bodyPage > 0 || m.index > 0) dashboard::hit(s, controls.before, Page, -1);
  if (bodyPage + 1 < pages || m.index + 1 < m.total) dashboard::hit(s, controls.after, Page, 1);
  if (pages > 1) {
    std::snprintf(label, sizeof(label), tr(STR_RT_TEXT_PAGE), bodyPage + 1, pages);
    text(s, makeRect(content.x, content.bottom() + 4, content.width, 28), label, 4);
  }
  return pages;
}
void message(toybox::Screen& s, const char* copy, bool confirm) {
  const auto b = begin(s, tr(STR_QUOTES_APP));
  const bool wide = b.width > b.height;
  text(s, makeRect(b.x + 12, b.y + 20, b.width - 24, b.height - (confirm && !wide ? 300 : 160)), copy, 4, 8);
  if (confirm) {
    if (wide) {
      const int half = (b.width - 12) / 2;
      button(s, makeRect(b.x, b.bottom() - 60, half, 58), tr(STR_RT_DELETE_QUOTE), ConfirmRemove);
      button(s, makeRect(b.x + half + 12, b.bottom() - 60, half, 58), tr(STR_RT_KEEP_QUOTE), Cancel, true);
    } else {
      button(s, makeRect(b.x, b.bottom() - 216, b.width, 58), tr(STR_RT_DELETE_QUOTE), ConfirmRemove);
      button(s, makeRect(b.x, b.bottom() - 116, b.width, 64), tr(STR_RT_KEEP_QUOTE), Cancel, true);
    }
  } else
    button(s, makeRect(b.x, b.bottom() - 60, b.width, 58), tr(STR_UI_BACK_587EAC), Back);
}
}  // namespace quotesui
