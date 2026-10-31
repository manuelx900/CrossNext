#include "MurdleScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/GamePanel.h"
#include "../ui/PageIndicator.h"
#include "../ui/PixelMenu.h"
#include "../ui/ToyboxIcons.h"
#include "MurdleArt.h"
#include "MurdlePeople.h"
#include "MurdleSession.h"
#include "MurdleText.h"
namespace murdleui {
namespace {
using fui::makeRect;
using murdle::Cat;
using murdle::Mark;
ClueLayout clueLayout;
void text(toybox::Screen& s, fui::Rect r, const char* t, int font = 4, int count = 1,
          fui::TextAlign align = fui::TextAlign::Left, bool inverse = false) {
  if (r.empty() || !t) return;
  fui::TextStyle style;
  style.align = align;
  style.color = inverse ? fui::Color::White : fui::Color::Black;
  const int candidates[] = {font, font == 2 ? 1 : 0, 0, 3, 4};
  for (int f : candidates) {
    int h = s.target().lineHeight(f);
    if (h <= 0 || h > r.height || h > s.target().lineHeight(font) || (font == 4 && f != 4)) continue;
    style.font = f;
    style.maxLines = std::min(count, std::max(1, int(r.height) / h));
    bool complete = true;
    const char* at = t;
    fui::layoutText(s.target(), r, t, style, [&](const char* run, fui::Rect) {
      while (*at == ' ' || *at == '\n') ++at;
      auto n = std::strlen(run);
      if (std::strncmp(at, run, n))
        complete = false;
      else
        at += n;
    });
    while (*at == ' ' || *at == '\n') ++at;
    if (complete && !*at) break;
  }
  s.target().text(r, t, style);
}
void caps(toybox::Screen& s, fui::Rect r, const char* t, int font = 0, int lines = 1,
          fui::TextAlign align = fui::TextAlign::Left, bool inverse = false) {
  char copy[192];
  pixelui::copyCaps(copy, sizeof(copy), t);
  text(s, r, copy, font, lines, align, inverse);
}
fui::Rect begin(toybox::Screen& s, const char* title) {
  pixelui::begin(s);
  pixelui::Header h;
  h.title = title;
  h.back = s.device().hasTouch;
  pixelui::header(s, h);
  backui::publish(h.back, pixelui::geometry(s.device()).back);
  return s.body();
}
void check(toybox::Screen& s, fui::Rect r, bool inverse = false) {
  const int side = std::min(r.width, r.height);
  r = makeRect(r.x + (r.width - side) / 2, r.y + (r.height - side) / 2, side, side);
  auto ink = inverse ? dashboard::paper : dashboard::ink;
  const int x = r.x, y = r.y, w = r.width, h = r.height, t = std::max(3, w / 8);
  s.target().line({int16_t(x + w / 6), int16_t(y + h / 2)}, {int16_t(x + w * 2 / 5), int16_t(y + h * 3 / 4)}, t, ink);
  s.target().line({int16_t(x + w * 2 / 5), int16_t(y + h * 3 / 4)}, {int16_t(x + w * 5 / 6), int16_t(y + h / 5)}, t,
                  ink);
}
void cross(toybox::Screen& s, fui::Rect r) {
  const int side = std::min(r.width, r.height);
  r = makeRect(r.x + (r.width - side) / 2, r.y + (r.height - side) / 2, side, side);
  const int t = std::max(2, int(r.width) / 9);
  s.target().line({r.x, r.y}, {int16_t(r.right()), int16_t(r.bottom())}, t, dashboard::ink);
  s.target().line({int16_t(r.right()), r.y}, {r.x, int16_t(r.bottom())}, t, dashboard::ink);
}
void heart(toybox::Screen& s, fui::Rect r, bool full) {
  murdleart::draw(s, r, full ? murdleart::HeartFull : murdleart::HeartEmpty);
}

void hearts(toybox::Screen& s, fui::Rect r, int lives, bool count = true) {
  const int side = std::min(int(r.height) - 8, count ? int(r.width) / 7 : int(r.width) / 4),
            gap = std::max(4, side / 4);
  const int start = r.x + (r.width - (3 * side + 2 * gap + (count ? side * 2 : 0))) / 2;
  for (int n = 0; n < 3; ++n)
    heart(s, makeRect(start + n * (side + gap), r.y + (r.height - side) / 2, side, side), n < lives);
  if (count) {
    char t[24];
    std::snprintf(t, sizeof(t), "%d/3", lives);
    text(s, makeRect(start + 3 * (side + gap), r.y, side * 2, r.height), t, 0);
  }
}
void button(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, int value = 0,
            bool primary = false, gamepanel::Mark icon = gamepanel::Mark::Back, bool hasIcon = false,
            bool enabled = true) {
  primary = s.frame().buttonSelected(action, value, primary);
  if (primary)
    lightui::surface(s.target(), r, dashboard::ink);
  else
    dashboard::frame(s, r, 2);
  if (hasIcon) gamepanel::mark(s, makeRect(r.x + 12, r.y + (r.height - 36) / 2, 36, 36), icon, primary);
  caps(s, makeRect(r.x + (hasIcon ? 64 : 8), r.y + 4, r.width - (hasIcon ? 96 : 16), r.height - 8), label,
       r.height < 56 ? 0 : 1, 2, hasIcon ? fui::TextAlign::Left : fui::TextAlign::Center, primary);
  if (hasIcon) dashboard::chevron(s.target(), makeRect(r.right() - 30, r.y, 24, r.height), primary);
  if (enabled) s.frame().hit(r, action, value, fui::InputTouch);
}
void tabs(toybox::Screen& s, fui::Rect band, const char* const* labels, int count, int active, fui::ActionId action) {
  for (int n = 0; n < count; ++n) {
    if (chrome::lightHeader) {
      dashboard::segment(s, band, n, count, labels[n], action, n, n == active, band.height < 56 ? 0 : 1);
      continue;
    }
    const auto r = makeRect(band.x + n * band.width / count, band.y,
                            (n + 1) * band.width / count - n * band.width / count, band.height);
    button(s, r, labels[n], action, n, n == active);
  }
}
void page(toybox::Screen& s, fui::Rect r, int current, int pages, fui::ActionId action = ActionPage) {
  if (r.width >= 200) {
    pageui::build(s, r, current + 1, pages, action, -1, action, 1);
    return;
  }
  auto left = makeRect(r.x, r.y, 44, 44), right = makeRect(r.right() - 44, r.y, 44, 44);
  dashboard::frame(s, left, 2);
  dashboard::frame(s, right, 2);
  for (int n = 0; n < 2; ++n) {
    auto box = n ? right : left;
    int sign = n ? 1 : -1, cx = box.x + 22, cy = box.y + 22;
    s.target().line({int16_t(cx - sign * 3), int16_t(cy - 7)}, {int16_t(cx + sign * 3), int16_t(cy)}, 3,
                    dashboard::ink);
    s.target().line({int16_t(cx + sign * 3), int16_t(cy)}, {int16_t(cx - sign * 3), int16_t(cy + 7)}, 3,
                    dashboard::ink);
  }
  char copy[20];
  std::snprintf(copy, sizeof(copy), "%d/%d", current + 1, pages);
  text(s, makeRect(left.right(), r.y, right.x - left.right(), 44), copy, 3, 1, fui::TextAlign::Center);
  if (current > 0) s.frame().hit(left, action, -1, fui::InputTouch | fui::InputPrev);
  if (current + 1 < pages) s.frame().hit(right, action, 1, fui::InputTouch | fui::InputNext);
}
void legend(toybox::Screen& s, fui::Rect r) {
  for (int n = 0; n < 3; ++n) {
    auto box = makeRect(r.x + n * r.width / 3, r.y, r.width / 3, r.height);
    if (!n)
      caps(s, box, tr(STR_MR_EMPTY_SHORT), 3, 2);
    else {
      auto mark = makeRect(box.x + 2, box.y + (box.height - 20) / 2, 20, 20);
      if (n == 1)
        cross(s, mark);
      else
        check(s, mark);
      caps(s, makeRect(box.x + 28, box.y, box.width - 30, box.height),
           n == 1 ? tr(STR_MR_NO_SHORT) : tr(STR_MR_YES_SHORT), 3);
    }
  }
}
void portrait(toybox::Screen& s, fui::Rect r, const murdle::Puzzle& p, int item) {
  static constexpr const fui::BitmapRef* images[] = {
      &murdleart::Person00, &murdleart::Person01, &murdleart::Person02, &murdleart::Person03, &murdleart::Person04,
      &murdleart::Person05, &murdleart::Person06, &murdleart::Person07, &murdleart::Person08, &murdleart::Person09,
      &murdleart::Person10, &murdleart::Person11, &murdleart::Person12, &murdleart::Person13, &murdleart::Person14,
      &murdleart::Person15, &murdleart::Person16, &murdleart::Person17, &murdleart::Person18, &murdleart::Person19,
      &murdleart::Person20, &murdleart::Person21, &murdleart::Person22, &murdleart::Person23, &murdleart::Person24,
      &murdleart::Person25, &murdleart::Person26, &murdleart::Person27, &murdleart::Person28, &murdleart::Person29};
  murdleart::draw(s, r, *images[murdlepeople::portrait(p, item)]);
}
void illustration(toybox::Screen& s, fui::Rect r, const murdle::Puzzle& p, int category, int item) {
  static_assert(sizeof(murdleart::weapons) / sizeof(murdleart::weapons[0]) == murdle::kWeaponCount);
  static_assert(sizeof(murdleart::places) / sizeof(murdleart::places[0]) == murdle::kPlaceCount);
  static_assert(sizeof(murdleart::motives) / sizeof(murdleart::motives[0]) == murdle::kMotiveCount);
  switch (static_cast<Cat>(category)) {
    case Cat::Suspect:
      portrait(s, r, p, item);
      break;
    case Cat::Weapon:
      murdleart::draw(s, r, *murdleart::weapons[p.cast[category][item]]);
      break;
    case Cat::Location:
      murdleart::draw(s, r, *murdleart::places[p.cast[category][item]]);
      break;
    case Cat::Motive: {
      const int entry = p.cast[category][item];
      if (entry >= 0 && entry < murdle::kMotiveCount) {
        const auto* art = murdleart::motiveById(murdle::kMotives[entry].name);
        if (art) murdleart::draw(s, r, *art);
      }
    } break;
  }
}
void illustratedChoice(toybox::Screen& s, fui::Rect r, const murdle::Puzzle& p, int category, int item, bool selected) {
  dashboard::frame(s, r, selected ? 3 : 1);
  char label[64];
  pixelui::copyCaps(label, sizeof(label), murdletext::label(p, category, item));
  fui::TextStyle style;
  style.font = 3;
  const int labelH = s.target().measureText(3, label, style).width <= r.width - 6 ? 24 : 44;
  const auto name = makeRect(r.x + 3, r.bottom() - labelH - 2, r.width - 6, labelH);
  if (selected) lightui::surface(s.target(), name, dashboard::ink);
  const auto picture = makeRect(r.x + 4, r.y + 4, r.width - 8, r.height - labelH - 10);
  illustration(s, picture, p, category, item);
  caps(s, name, label, 3, 2, fui::TextAlign::Center, selected);
  s.frame().hit(r, ActionPick, category * 8 + item, fui::InputTouch);
}
void caseStatus(toybox::Screen& s, fui::Rect r, int number, int wrong, bool solution = false) {
  dashboard::frame(s, r, 2);
  char t[40];
  std::snprintf(t, sizeof(t), tr(STR_MR_CASE), number);
  caps(s, makeRect(r.x + 8, r.y + 2, r.width / 2 - 16, r.height - 4), solution ? tr(STR_MR_SOLUTION) : t, 0, 1,
       fui::TextAlign::Center);
  s.target().line({int16_t(r.x + r.width / 2), int16_t(r.y + 7)}, {int16_t(r.x + r.width / 2), int16_t(r.bottom() - 7)},
                  1, dashboard::ink);
  hearts(s, makeRect(r.x + r.width / 2, r.y, r.width / 2, r.height), murdlesession::lives(wrong));
}
int lines(toybox::Screen& s, const char* copy, int width) {
  int count = 0;
  fui::TextStyle style;
  style.font = 4;
  style.maxLines = 24;
  fui::layoutText(s.target(), makeRect(0, 0, width, 1200), copy, style, [&](const char*, fui::Rect) { ++count; });
  return count;
}
void infoBox(toybox::Screen& s, fui::Rect r, const char* title, const char* body, gamepanel::Mark icon,
             bool separator = false) {
  dashboard::frame(s, r, 2);
  gamepanel::mark(s, makeRect(r.x + 10, r.y + 12, 40, 40), icon);
  const int inset = separator ? 80 : 64;
  if (separator)
    s.target().line({int16_t(r.x + 64), int16_t(r.y + 12)}, {int16_t(r.x + 64), int16_t(r.bottom() - 12)}, 1,
                    dashboard::ink);
  caps(s, makeRect(r.x + inset, r.y + 6, r.width - inset - 12, 30), title, 0, 1);
  text(s, makeRect(r.x + inset, r.y + 38, r.width - inset - 12, r.height - 42), body, 4, 3);
}
CaseReport clues(toybox::Screen& s, const CaseModel& m, fui::Rect area) {
  CaseReport report;
  const auto& p = *m.puzzle;
  const bool wide = area.width > area.height * 2;
  const int leftW = wide ? area.width * 2 / 3 : area.width;
  const int ruleH = p.tier == murdle::Tier::Impossible ? 68 : 0;
  const auto footer = wide ? makeRect(area.x + leftW + 12, area.bottom() - 40, area.width - leftW - 12, 40)
                           : makeRect(area.x, area.bottom() - 44, area.width, 44);
  const auto crime = wide ? makeRect(footer.x, area.y + 52, footer.width, footer.y - area.y - 60)
                          : makeRect(area.x, footer.y - 134, area.width, 126);
  const auto content = wide ? makeRect(area.x, area.y + 52 + ruleH, leftW, area.height - 52 - ruleH)
                            : makeRect(area.x, area.y + 52 + ruleH, area.width, crime.y - area.y - 60 - ruleH);
  int16_t stops[murdle::kMaxClues + 1]{};
  int pages = 0, used = 0;
  stops[0] = 0;
  for (int n = 0; n < p.clueCount; ++n) {
    char copy[murdletext::kLineMax];
    murdletext::clueLine(p, n, copy, sizeof(copy));
    const int h = std::max(82, lines(s, copy, content.width - 136) * s.target().lineHeight(4) + 20);
    if (used && used + h > content.height) {
      stops[++pages] = n;
      used = 0;
    }
    used += std::min(h, int(content.height)) + 8;
  }
  stops[++pages] = p.clueCount;
  report.pages = pages;
  report.page = std::clamp(m.page, 0, pages - 1);
  char label[64];
  std::snprintf(label, sizeof(label), tr(STR_MR_EVIDENCE), report.page + 1, pages);
  pixelui::text(s.target(), makeRect(area.x, area.y, area.width - 144, 44), label, toybox::kUiFont, fui::Color::Black,
                fui::TextAlign::Left);
  page(s, makeRect(area.right() - 136, area.y, 136, 44), report.page, pages);
  if (ruleH) text(s, makeRect(area.x + 4, area.y + 48, leftW - 8, ruleH - 4), tr(STR_MR_LIAR_RULE), 4, 3);
  int y = content.y;
  clueLayout = {};
  for (int n = stops[report.page]; n < stops[report.page + 1]; ++n) {
    char copy[murdletext::kLineMax];
    murdletext::clueLine(p, n, copy, sizeof(copy));
    const int h = std::min(std::max(82, lines(s, copy, content.width - 136) * s.target().lineHeight(4) + 20),
                           int(content.bottom() - y));
    const auto r = makeRect(content.x, y, content.width, h);
    dashboard::frame(s, r, 2);
    const bool reviewed = m.struck & (1u << n);
    const int centerY = r.y + (r.height - 36) / 2;
    const auto tick = makeRect(r.x + 12, centerY, 36, 36);
    if (reviewed) {
      lightui::surface(s.target(), tick, dashboard::ink);
      check(s, tick, true);
    } else
      dashboard::frame(s, tick, 2);
    s.target().line({int16_t(r.x + 64), int16_t(r.y + 10)}, {int16_t(r.x + 64), int16_t(r.bottom() - 10)}, 1,
                    dashboard::ink);
    dashboard::frame(s, makeRect(r.x + 80, centerY, 36, 36), 2);
    std::snprintf(label, sizeof(label), "%d", n + 1);
    text(s, makeRect(r.x + 80, centerY, 36, 36), label, 0, 1, fui::TextAlign::Center);
    text(s, makeRect(r.x + 124, r.y + 8, r.width - 136, r.height - 16), copy, 4, 5);
    if (!m.solved) s.frame().hit(r, ActionClue, n, fui::InputTouch);
    clueLayout.top[clueLayout.count] = r.y;
    clueLayout.index[clueLayout.count++] = n;
    y = r.bottom() + 8;
  }
  clueLayout.top[clueLayout.count] = y - 8;
  char crimeText[murdletext::kLineMax];
  int murder = 0;
  for (int n = 0; n < p.clueCount; ++n)
    if (p.clues[n].anchor == murdle::Anchor::Murderer) {
      murder = n;
      break;
    }
  murdletext::clueLine(p, murder, crimeText, sizeof(crimeText));
  if (wide) {
    dashboard::frame(s, crime, 2);
    caps(s, makeRect(crime.x + 8, crime.y + 4, crime.width - 16, 44), tr(STR_MR_CRIME_EVIDENCE), 0, 2);
    text(s, makeRect(crime.x + 8, crime.y + 52, crime.width - 16, crime.height - 56), crimeText, 4, 6);
  } else
    infoBox(s, crime, tr(STR_MR_CRIME_EVIDENCE), crimeText, gamepanel::Mark::Search, true);
  text(s, footer, tr(STR_MR_MARK_USED), 4, 2);
  return report;
}
CaseReport grid(toybox::Screen& s, const CaseModel& m, fui::Rect area) {
  CaseReport report;
  const auto& p = *m.puzzle;
  report.pages = p.shape.blocks();
  report.page = std::clamp(m.page, 0, report.pages - 1);
  int pair = 0, a = 0, b = 1;
  for (int i = 0; i < p.shape.cats; ++i)
    for (int j = i + 1; j < p.shape.cats; ++j)
      if (pair++ == report.page) {
        a = i;
        b = j;
      }
  char copy[160];
  std::snprintf(copy, sizeof(copy), tr(STR_MR_PAIR), murdletext::categoryName(a), murdletext::categoryName(b));
  const bool wide = area.width > area.height * 2;
  const int boardW = wide ? area.width * 3 / 4 : area.width;
  const auto side = makeRect(area.x + boardW + 16, area.y, area.width - boardW - 16, area.height);
  caps(s, wide ? makeRect(side.x, side.y, side.width, 56) : makeRect(area.x, area.y, area.width - 146, 44), copy, 0, 2);
  page(s, wide ? makeRect(side.x, side.y + 60, side.width, 44) : makeRect(area.right() - 140, area.y, 140, 44),
       report.page, report.pages);
  auto& g = report.grid;
  g.valid = true;
  g.groups = 1;
  g.items = p.shape.items;
  g.rowCat[0] = a;
  g.colCat[0] = b;
  g.gutter = std::min(152, boardW / 3);
  g.headerH = wide ? 76 : 100;
  g.cell = (int(area.height) - g.headerH - (wide ? 0 : 216)) / g.items;
  g.cellWidth = (boardW - g.gutter) / g.items;
  g.originX = area.x + g.gutter;
  g.originY = area.y + (wide ? 0 : 60) + g.headerH;
  for (int n = 0; n < g.items; ++n) {
    auto top = makeRect(g.cellX(n), g.originY - g.headerH, g.cellWidth, g.headerH);
    dashboard::frame(s, top, 1);
    illustration(s, makeRect(top.x + 4, top.y + 4, top.width - 8, top.height - 50), p, b, n);
    caps(s, makeRect(top.x + 3, top.bottom() - 44, top.width - 6, 42), murdletext::label(p, b, n), 3, 2,
         fui::TextAlign::Center);
    auto left = makeRect(area.x, g.cellY(n), g.gutter, g.cell);
    dashboard::frame(s, left, 1);
    const int pictureW = std::min(int(left.height) - 8, 48);
    illustration(s, makeRect(left.x + 4, left.y + 4, pictureW, left.height - 8), p, a, n);
    caps(s, makeRect(left.x + pictureW + 8, left.y + 4, left.width - pictureW - 12, left.height - 8),
         murdletext::label(p, a, n), 3, 2);
  }
  for (int r = 0; r < g.items; ++r)
    for (int c = 0; c < g.items; ++c) {
      auto box = makeRect(g.cellX(c), g.cellY(r), g.cellWidth, g.cell);
      const auto mark = m.marks->shown(a, r, b, c);
      if (mark == Mark::Yes) {
        lightui::surface(s.target(), box, dashboard::ink);
        check(
            s,
            box.inset(fui::Insets{int16_t(g.cell / 4), int16_t(g.cell / 4), int16_t(g.cell / 4), int16_t(g.cell / 4)}),
            true);
      } else {
        dashboard::frame(s, box, 1);
        if (mark == Mark::No)
          cross(s, box.inset(fui::Insets{int16_t(g.cell / 3), int16_t(g.cell / 3), int16_t(g.cell / 3),
                                         int16_t(g.cell / 3)}));
      }
    }
  if (!m.solved) {
    const auto grid = makeRect(g.originX, g.originY, g.cellWidth * g.items, g.cell * g.items);
    s.frame().hit(grid, ActionGrid, 0, fui::InputTouch | fui::InputPointerOnly);
    s.frame().navigationGrid(grid, g.items, g.items);
  }
  const auto tip = wide ? makeRect(side.x, side.y + 112, side.width, 44)
                        : makeRect(area.x, g.originY + g.items * g.cell + 12, area.width, 80);
  if (m.lastA >= 0 && m.marks->shown(m.lastA, m.lastIA, m.lastB, m.lastIB) == Mark::Yes) {
    std::snprintf(copy, sizeof(copy), "%s + %s", murdletext::label(p, m.lastA, m.lastIA),
                  murdletext::label(p, m.lastB, m.lastIB));
    if (wide)
      caps(s, tip, copy, 3, 2);
    else {
      dashboard::frame(s, tip, 2);
      caps(s, makeRect(tip.x + 8, tip.y + 4, tip.width - 16, 36), copy, 1, 1, fui::TextAlign::Center);
      caps(s, makeRect(tip.x + 8, tip.y + 42, tip.width - 16, 30), tr(STR_MR_RELATION_CONFIRMED), 0, 1,
           fui::TextAlign::Center);
    }
  } else if (wide)
    caps(s, tip, m.notice && m.notice[0] ? m.notice : tr(STR_MR_DISTINCT), 3, 2);
  else
    infoBox(s, tip, tr(STR_MR_CROSS_CLUES), m.notice && m.notice[0] ? m.notice : tr(STR_MR_DISTINCT),
            gamepanel::Mark::Help);
  if (wide) {
    caps(s, makeRect(side.x, area.bottom() - 100, side.width, 24), tr(STR_MR_EMPTY_SHORT), 3);
    const auto no = makeRect(side.x, area.bottom() - 74, side.width / 2, 24);
    cross(s, makeRect(no.x, no.y + 2, 20, 20));
    caps(s, makeRect(no.x + 28, no.y, no.width - 28, 24), tr(STR_MR_NO_SHORT), 3);
    check(s, makeRect(no.right(), no.y + 2, 20, 20));
    caps(s, makeRect(no.right() + 28, no.y, no.width - 28, 24), tr(STR_MR_YES_SHORT), 3);
  } else
    legend(s, makeRect(area.x, area.bottom() - 48, area.width - 112, 44));
  button(s,
         wide ? makeRect(side.x, area.bottom() - 44, side.width, 44)
              : makeRect(area.right() - 104, area.bottom() - 48, 104, 44),
         tr(STR_UNDO), ActionUndo, 0, false, gamepanel::Mark::Back, false, m.canUndo && !m.solved);
  return report;
}
CaseReport dossier(toybox::Screen& s, const CaseModel& m, fui::Rect area) {
  CaseReport report;
  const auto& p = *m.puzzle;
  report.pages = p.shape.cats;
  report.page = std::clamp(m.page, 0, report.pages - 1);
  const bool wide = area.width > area.height * 2;
  const char* labels[] = {tr(STR_MR_PEOPLE), murdletext::categoryName(1), murdletext::categoryName(2),
                          murdletext::categoryName(3)};
  tabs(s, makeRect(area.x, area.y, area.width, 44), labels, p.shape.cats, report.page, ActionInfoCategory);
  bool compact = p.shape.items == 4;
  if (!report.page) {
    for (int n = 0; n < p.shape.items; ++n) {
      char attr[160];
      murdletext::suspectAttributes(p, n, attr, sizeof(attr));
      if (lines(s, attr, area.width - 132) > 2) compact = true;
    }
  }
  const int cols = wide ? 2 : 1, rows = wide ? (p.shape.items + 1) / 2 : p.shape.items;
  const int height = wide ? (int(area.height) - 52 - 8 * (rows - 1)) / rows
                          : (int(area.height) - (compact ? 52 : 160) - 8 * (rows - 1)) / rows;
  const int width = (int(area.width) - 8 * (cols - 1)) / cols;
  for (int n = 0; n < p.shape.items; ++n) {
    const auto r = makeRect(area.x + n % cols * (width + 8), area.y + 52 + n / cols * (height + 8), width, height);
    dashboard::frame(s, r, 2);
    const auto picture = makeRect(r.x + 4, r.y + 4, wide ? 76 : 100, r.height - 8);
    illustration(s, picture, p, report.page, n);
    const int x = r.x + (wide ? 86 : 120);
    caps(s, makeRect(x, r.y + 2, r.right() - x - 8, wide || compact ? 24 : 34), murdletext::label(p, report.page, n),
         wide || compact ? 0 : 1);
    char attr[160];
    if (!report.page)
      murdletext::suspectAttributes(p, n, attr, sizeof(attr));
    else
      std::snprintf(attr, sizeof(attr), "%s", murdletext::trait(p, report.page, n));
    text(s, makeRect(x, r.y + (wide || compact ? 26 : 44), r.right() - x - 8, r.height - (wide || compact ? 30 : 50)),
         attr, 4, 5);
  }
  if (!wide && !compact)
    infoBox(s, makeRect(area.x, area.bottom() - 94, area.width, 94), tr(STR_MR_COMPARE), tr(STR_MR_COMPARE_HELP),
            gamepanel::Mark::Search);
  return report;
}
}  // namespace

bool cellAt(const GridLayout& g, int x, int y, GridCell& out) {
  if (!g.valid || g.cell <= 0 || x < g.originX || y < g.originY) return false;
  const int col = (x - g.originX) / (g.cellWidth ? g.cellWidth : g.cell), row = (y - g.originY) / g.cell,
            span = g.groups * g.items;
  if (col >= span || row >= span || !g.blockLive(row / g.items, col / g.items)) return false;
  out = {g.rowCat[row / g.items], row % g.items, g.colCat[col / g.items], col % g.items};
  return true;
}
ClueLayout lastClueLayout() { return clueLayout; }
bool AccuseModel::complete() const {
  if (!puzzle) return false;
  for (int n = 0; n < puzzle->shape.cats; ++n)
    if (picks[n] >= puzzle->shape.items) return false;
  return true;
}
const char* tierName(murdle::Tier tier) {
  static constexpr StrId keys[] = {StrId::STR_UI_ELEMENTARY_78FEC8, StrId::STR_UI_NOSY_FBE3A8,
                                   StrId::STR_UI_HARD_BOILED_1ABD76, StrId::STR_UI_IMPOSSIBLE_0109E7};
  return I18N.get(keys[int(tier)]);
}
const char* tierShape(murdle::Tier tier) {
  static constexpr const char* shapes[] = {"3 × 3", "3 × 4", "4 × 4", "4 × 4"};
  return shapes[int(tier)];
}
CaseReport buildCase(toybox::Screen& s, const CaseModel& m) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  const bool wide = b.width > b.height;
  if (!m.puzzle || !m.marks) return {};
  caseStatus(s, makeRect(b.x, b.y, wide ? b.width / 3 - 8 : b.width, wide ? 44 : 36), m.caseNumber, m.wrongAccusations,
             m.solution);
  const char* labels[] = {tr(STR_MR_CLUES), tr(STR_MR_TABLE), tr(STR_MR_DOSSIER)};
  tabs(s, wide ? makeRect(b.x + b.width / 3, b.y, b.width - b.width / 3, 44) : makeRect(b.x, b.y + 44, b.width, 44),
       labels, 3, int(m.face), ActionFace);
  const auto area =
      wide ? makeRect(b.x, b.y + 56, b.width, b.height - 114) : makeRect(b.x, b.y + 104, b.width, b.height - 174);
  CaseReport report;
  switch (m.face) {
    case Face::Clues:
      report = clues(s, m, area);
      break;
    case Face::Grid:
      report = grid(s, m, area);
      break;
    case Face::Info:
      report = dossier(s, m, area);
      break;
  }
  if (!m.solved)
    button(s, makeRect(b.x, b.bottom() - 58, b.width, 58), tr(STR_UI_ACCUSE_12BFD2), ActionAccuse, 0, true,
           gamepanel::Mark::Accuse, true);
  else
    button(s, makeRect(b.x, b.bottom() - 58, b.width, 58), tr(STR_MR_BACK_RESULT), ActionBack, 0, false,
           gamepanel::Mark::Back, true);
  return report;
}
void buildMenu(toybox::Screen& s, const MenuModel& m) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  const bool wide = b.width > b.height;
  const auto hero = wide ? makeRect(b.x, b.y, b.width / 2 - 12, b.height) : makeRect(b.x, b.y, b.width, b.height - 230);
  dashboard::frame(s, hero, 2);
  const int artW = std::min(190, int(hero.width) * 44 / 100);
  murdleart::draw(s, makeRect(hero.x + 10, hero.y + 10, artW, hero.height - 130), murdleart::Menu);
  const int x = hero.x + artW + 24, w = hero.right() - x - 12;
  caps(s, makeRect(x, hero.y + 16, w, 112), tr(STR_MR_MYSTERY), 2, 3);
  s.target().line({int16_t(x), int16_t(hero.y + 138)}, {int16_t(hero.right() - 12), int16_t(hero.y + 138)}, 2,
                  dashboard::ink);
  char copy[64];
  std::snprintf(copy, sizeof(copy), tr(STR_MR_CASE_TIER), std::max(1, m.caseNumber),
                tierName(m.hasCase && m.puzzle ? m.puzzle->tier : m.tier));
  caps(s, makeRect(x, hero.y + 148, w, 42), copy, 0, 2);
  hearts(s, makeRect(x, hero.y + (wide ? 190 : 202), w, wide ? 34 : 52), murdlesession::lives(m.wrongAccusations),
         false);
  std::snprintf(copy, sizeof(copy),
                murdlesession::lives(m.wrongAccusations) == 1 ? tr(STR_MR_ONE_LIFE) : tr(STR_MR_LIVES),
                murdlesession::lives(m.wrongAccusations));
  caps(s, makeRect(x, hero.y + (wide ? 228 : 260), w, 40), copy, 1, 1, fui::TextAlign::Center);
  const int items = m.puzzle ? m.puzzle->shape.items : 3;
  const char* labels[] = {tr(STR_MR_PEOPLE), murdletext::categoryName(1), murdletext::categoryName(2)};
  const freeink::Icon* icons[] = {&icon_murdle_suspects_24, &icon_murdle_weapons_24, &icon_murdle_places_24};
  const int rowY = hero.bottom() - 112;
  s.target().line({int16_t(hero.x + 10), int16_t(rowY)}, {int16_t(hero.right() - 10), int16_t(rowY)}, 2,
                  dashboard::ink);
  for (int n = 0; n < 3; ++n) {
    auto r = makeRect(hero.x + n * hero.width / 3, rowY + 10, hero.width / 3, 100);
    if (n) s.target().line({r.x, r.y}, {r.x, int16_t(r.bottom() - 8)}, 1, dashboard::ink);
    if (!n) {
      for (int who = 0; who < 3; ++who)
        dashboard::icon(s, makeRect(r.x + (r.width - 66) / 2 + who * 22, r.y + 2, 22, 34), *icons[n]);
    } else
      dashboard::icon(s, makeRect(r.x + (r.width - 38) / 2, r.y, 38, 38), *icons[n]);
    std::snprintf(copy, sizeof(copy), "%d", items);
    caps(s, makeRect(r.x + 6, r.y + 34, r.width - 12, 30), copy, 1, 1, fui::TextAlign::Center);
    caps(s, makeRect(r.x + 6, r.y + 68, r.width - 12, 24), labels[n], 3, 1, fui::TextAlign::Center);
  }
  const auto actions = wide ? makeRect(hero.right() + 20, b.y, b.right() - hero.right() - 20, b.height)
                            : makeRect(b.x, hero.bottom() + 14, b.width, 216);
  const int h = std::min(68, (int(actions.height) - 16) / 3);
  button(s, makeRect(actions.x, actions.y, actions.width, h),
         m.hasCase ? tr(STR_MURDLE_RESUME_CASE) : tr(STR_UI_OPEN_A_CASE_6474DD),
         m.hasCase ? ActionPlay : ActionCampaign, 0, true, gamepanel::Mark::Play, true);
  button(s, makeRect(actions.x, actions.y + h + 8, actions.width, h), tr(STR_MR_CHOOSE_CASE), ActionCampaign, 0, false,
         gamepanel::Mark::Grid, true);
  button(s, makeRect(actions.x, actions.y + 2 * (h + 8), actions.width, h), tr(STR_MR_HOW_PLAY), ActionHowTo, 0, false,
         gamepanel::Mark::Help, true);
}
void buildCampaign(toybox::Screen& s, const murdlecampaign::Progress& progress, int current, int selected, int active,
                   murdle::Tier filter) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  const bool wide = b.width > b.height;
  const auto sidebar = wide ? makeRect(b.x, b.y, 220, b.height) : b;
  caps(s, makeRect(sidebar.x, sidebar.y, wide ? sidebar.width : sidebar.width - 148, 56), tr(STR_MR_CHOOSE_CASE), 0, 2);
  button(s,
         wide ? makeRect(sidebar.x, sidebar.bottom() - 48, sidebar.width, 48)
              : makeRect(sidebar.right() - 140, sidebar.y, 140, 48),
         tr(STR_MR_RESET), ActionResetProgress);
  caps(s, makeRect(sidebar.x, b.y + (wide ? 48 : 60), sidebar.width / 2, 34), tr(STR_MR_SOLVED), 0);
  char copy[64];
  std::snprintf(copy, sizeof(copy), "%d / %d", progress.count(), murdlecampaign::kCaseCount);
  text(s, makeRect(sidebar.x + sidebar.width / 2, b.y + (wide ? 48 : 60), sidebar.width / 2, 34), copy, 0, 1,
       fui::TextAlign::Right);
  auto bar = makeRect(sidebar.x, b.y + (wide ? 84 : 98), sidebar.width, 12);
  dashboard::frame(s, bar, 2);
  if (progress.count())
    s.target().fill(
        makeRect(bar.x + 3, bar.y + 3, (bar.width - 6) * progress.count() / murdlecampaign::kCaseCount, bar.height - 6),
        dashboard::ink);
  caps(s, makeRect(sidebar.x, b.y + (wide ? 100 : 120), sidebar.width, 24), tr(STR_UI_DIFFICULTY_9106E9), 3);
  const int columns = wide ? 1 : 2, gap = 8;
  for (int n = 0; n < murdle::kTierCount; ++n) {
    const auto r = makeRect(sidebar.x + n % columns * (sidebar.width + gap) / columns,
                            b.y + (wide ? 128 : 150) + n / columns * (wide ? 48 : 52),
                            (sidebar.width - gap * (columns - 1)) / columns, 44);
    button(s, r, tierName(static_cast<murdle::Tier>(n)), ActionTier, n, n == static_cast<int>(filter));
  }
  current = std::clamp(current, 0, murdlecampaign::kPagesPerTier - 1);
  const int first = murdlecampaign::first(filter) + current * murdlecampaign::kCasesPerPage;
  const int end =
      std::min(first + murdlecampaign::kCasesPerPage, murdlecampaign::first(filter) + murdlecampaign::kCasesPerTier);
  const auto area = wide ? makeRect(sidebar.right() + 16, b.y, b.width - sidebar.width - 16, b.height)
                         : makeRect(b.x, b.y + 260, b.width, b.height - 260);
  std::snprintf(copy, sizeof(copy), tr(STR_MR_CASE_RANGE), first + 1, end);
  caps(s, makeRect(area.x, area.y, area.width, 32), copy, 0);
  const int gridH = area.height - (wide ? 88 : 104), cw = (area.width - 3 * gap) / 4, ch = (gridH - 3 * gap) / 4;
  for (int index = first; index < end; ++index) {
    const int n = index - first;
    const auto r = makeRect(area.x + n % 4 * (cw + gap), area.y + 40 + n / 4 * (ch + gap), cw, ch);
    const bool done = progress.cleared(index), playing = index == active && !done;
    if (playing)
      lightui::surface(s.target(), r, dashboard::ink);
    else
      dashboard::frame(s, r, index == selected ? 3 : 2);
    std::snprintf(copy, sizeof(copy), "%02d", index + 1);
    text(s, makeRect(r.x + 4, r.y + 2, r.width - 8, r.height - 44), copy, wide ? 3 : 2, 1, fui::TextAlign::Center,
         playing);
    const auto label = makeRect(r.x + 4, r.bottom() - (wide ? 42 : 44), r.width - 8, wide ? 40 : 42);
    if (done) {
      lightui::surface(s.target(), label, dashboard::ink);
      check(s, makeRect(r.right() - 19, r.y + 5, 14, 14));
      caps(s, label, tr(STR_MR_REPLAY_SHORT), 3, 1, fui::TextAlign::Center, true);
    } else {
      caps(s, label, playing ? tr(STR_MR_IN_PROGRESS) : tr(STR_MR_UNSOLVED), 3, 2, fui::TextAlign::Center, playing);
    }
    s.frame().hit(r, ActionOpenCampaign, index, fui::InputTouch);
  }
  page(s, makeRect(area.x, b.bottom() - 48, area.width, 48), current, murdlecampaign::kPagesPerTier,
       ActionCampaignPage);
}
void buildSettings(toybox::Screen& s, const SettingsModel& m) {
  auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  caps(s, makeRect(b.x, b.y, b.width, 56), tr(STR_MR_CHOOSE_CASE), 2);
  text(s, makeRect(b.x, b.y + 70, b.width, 80), tierName(m.tier), 4, 2);
  button(s, makeRect(b.x, b.bottom() - 58, b.width, 58), tr(STR_QUEST_BACK), ActionBack);
}
void buildAccuse(toybox::Screen& s, const AccuseModel& m) {
  const auto b = begin(s, tr(STR_UI_ACCUSE_12BFD2));
  if (!m.puzzle) return;
  const auto& p = *m.puzzle;
  const bool wide = b.width > b.height;
  const bool padded = chrome::lightHeader;
  caps(s, makeRect(b.x, b.y, b.width - 166, 44), tr(STR_MR_WHAT_HAPPENED), 2);
  hearts(s, makeRect(b.right() - 162, b.y, 162, 44), murdlesession::lives(m.wrongAccusations));
  const int leftW = wide ? b.width * 3 / 4 - 8 : b.width;
  const int cols = wide ? p.shape.cats : p.shape.cats == 4 ? 2 : 1;
  const int rows = wide ? 1 : (p.shape.cats + cols - 1) / cols;
  const int summaryH = padded ? 164 : 138;
  const int groupsH = wide ? b.height - 52 : b.height - 52 - summaryH - 108 - 16;
  const int h = (groupsH - 8 * (rows - 1)) / rows, gw = (leftW - 8 * (cols - 1)) / cols;
  static constexpr StrId cats[] = {StrId::STR_MR_WHO, StrId::STR_MR_WITH, StrId::STR_MR_WHERE, StrId::STR_MR_WHY};
  for (int c = 0; c < p.shape.cats; ++c) {
    const auto group = makeRect(b.x + c % cols * (gw + 8), b.y + 52 + c / cols * (h + 8), gw, h);
    dashboard::frame(s, group, 2);
    caps(s, makeRect(group.x + 6, group.y + 2, group.width - 12, wide ? 44 : 26), I18N.get(cats[c]), 0, 2);
    const int optionCols = wide ? 1 : p.shape.cats == 4 ? 2 : p.shape.items;
    const int optionRows = (p.shape.items + optionCols - 1) / optionCols, gap = 6;
    const int w = (group.width - 12 - gap * (optionCols - 1)) / optionCols;
    const int optionTop = wide ? 46 : 28;
    const int oh = (group.height - optionTop - 6 - gap * (optionRows - 1)) / optionRows;
    for (int i = 0; i < p.shape.items; ++i) {
      const auto box =
          makeRect(group.x + 6 + i % optionCols * (w + gap), group.y + optionTop + i / optionCols * (oh + gap), w, oh);
      illustratedChoice(s, box, p, c, i, m.picks[c] == i);
    }
  }
  const auto summary = wide ? makeRect(b.x + leftW + 14, b.y + 52, b.width - leftW - 14, b.height - 52)
                            : makeRect(b.x, b.bottom() - summaryH - 108, b.width, summaryH);
  const int previewH = wide ? summary.height - 108 : summary.height;
  dashboard::frame(s, makeRect(summary.x, summary.y, summary.width, previewH), 2);
  caps(s, makeRect(summary.x + 6, summary.y + (padded ? 8 : 2), summary.width - 12, 24), tr(STR_MR_YOUR_ACCUSATION), 0,
       1, fui::TextAlign::Center);
  static constexpr StrId placeholders[] = {StrId::STR_MR_PICK_PERSON, StrId::STR_MR_PICK_WEAPON,
                                           StrId::STR_MR_PICK_PLACE, StrId::STR_MR_PICK_MOTIVE};
  const int previewCols = wide ? 1 : p.shape.cats, previewRows = wide ? p.shape.cats : 1;
  const int tileW = (summary.width - 12 - 6 * (previewCols - 1)) / previewCols;
  const int tileTop = padded ? 40 : 28, tileGap = padded ? 8 : 4;
  const int tileH =
      padded ? (previewH - tileTop - 38 - (!wide && !m.complete() ? 24 : 0) - tileGap * (previewRows - 1)) / previewRows
             : (previewH - (wide ? 56 : 76) - 4 * (previewRows - 1)) / previewRows;
  for (int c = 0; c < p.shape.cats; ++c) {
    const auto tile = makeRect(summary.x + 6 + c % previewCols * (tileW + 6),
                               summary.y + tileTop + c / previewCols * (tileH + tileGap), tileW, tileH);
    dashboard::frame(s, tile, 1);
    const auto image = wide ? makeRect(tile.x + 3, tile.y + 3, tile.height - 6, tile.height - 6)
                            : makeRect(tile.x + (padded ? 6 : 3), tile.y + (padded ? 6 : 3),
                                       tile.width - (padded ? 12 : 6), tile.height - (padded ? 36 : 28));
    if (m.picks[c] < p.shape.items)
      illustration(s, image, p, c, m.picks[c]);
    else
      gamepanel::mark(s, image, gamepanel::Mark::Help);
    const auto label = wide ? makeRect(tile.x + tile.height, tile.y + 2, tile.width - tile.height - 4, tile.height - 4)
                            : makeRect(tile.x + (padded ? 6 : 2), tile.bottom() - (padded ? 28 : 24),
                                       tile.width - (padded ? 12 : 4), 22);
    caps(s, label, m.picks[c] < p.shape.items ? murdletext::label(p, c, m.picks[c]) : I18N.get(placeholders[c]), 3, 1,
         fui::TextAlign::Center);
  }
  if (!m.complete() && !wide)
    caps(s, makeRect(summary.x + 5, summary.y + previewH - 50, summary.width - 10, 24), tr(STR_MR_SELECT_ALL), 3, 1,
         fui::TextAlign::Center);
  caps(s, makeRect(summary.x + 5, summary.y + previewH - 26, summary.width - 10, 24), tr(STR_MR_WRONG_COST), 3, 1,
       fui::TextAlign::Center);
  const int actionsY = wide ? summary.bottom() - 108 : b.bottom() - 104;
  button(s, makeRect(summary.x, actionsY, summary.width, wide ? 52 : 48), tr(STR_MR_CONFIRM), ActionConfirm, 0, true,
         gamepanel::Mark::Play, !wide, m.complete());
  button(s, makeRect(summary.x, actionsY + 56, summary.width, wide ? 52 : 48), tr(STR_MR_BACK_INVESTIGATE),
         ActionKeepLooking, 0, false, gamepanel::Mark::Back, !wide);
}
namespace {
void verdictArt(toybox::Screen& s, fui::Rect r, bool right, bool failed) {
  if (right || failed) {
    murdleart::draw(s, r, right ? murdleart::Solved : murdleart::Failed);
    return;
  }
  const auto& art = murdleart::Wrong;
  murdleart::draw(s, r, art);
  // The source question mark ends at the cropped bitmap edge.
  const int scale = std::min((int(r.width) << 8) / art.width, (int(r.height) << 8) / art.height);
  const int x = r.x + (r.width - ((art.width * scale) >> 8)) / 2;
  const int y = r.y + (r.height - ((art.height * scale) >> 8)) / 2;
  s.target().fill(makeRect(x + ((202 * scale) >> 8), y, (45 * scale + 255) >> 8, (126 * scale + 255) >> 8),
                  dashboard::paper);
  const int side = (54 * scale) >> 8;
  gamepanel::mark(s, makeRect(x + ((189 * scale) >> 8), y + ((15 * scale) >> 8), side, side), gamepanel::Mark::Help);
}
}  // namespace
void buildVerdict(toybox::Screen& s, const VerdictModel& m) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  if (!m.puzzle) return;
  const bool failed = !m.right && murdlesession::lives(m.wrongAccusations) == 0, wide = b.width > b.height;
  if (wide) {
    const int leftW = b.width / 2 - 12;
    auto right = makeRect(b.x + leftW + 24, b.y, b.width - leftW - 24, b.height);
    caps(s, makeRect(b.x, b.y, leftW, 56),
         m.right  ? tr(STR_MR_RESOLVED)
         : failed ? tr(STR_MR_FAILED)
                  : tr(STR_MR_NOT_FIT),
         2, 1, fui::TextAlign::Center);
    if (m.right)
      text(s, makeRect(b.x, b.y + 60, leftW, 36), tr(STR_MR_TRUTH), 4, 1, fui::TextAlign::Center);
    else
      hearts(s, makeRect(b.x, b.y + 60, leftW, 50), murdlesession::lives(m.wrongAccusations), false);
    verdictArt(s, makeRect(b.x + 10, b.y + 118, leftW - 20, b.height - 126), m.right, failed);
    char copy[160];
    if (m.right) {
      uint8_t correct[4]{};
      for (int c = 0; c < m.puzzle->shape.cats; ++c) correct[c] = m.puzzle->assign[c][m.puzzle->murderRow];
      murdletext::accusationLine(*m.puzzle, correct, copy, sizeof(copy));
    } else if (failed)
      std::snprintf(copy, sizeof(copy), "%s", tr(STR_MR_FAILED_HELP));
    else
      murdletext::accusationLine(*m.puzzle, m.picks, copy, sizeof(copy));
    dashboard::frame(s, makeRect(right.x, right.y, right.width, 116), 2);
    text(s, makeRect(right.x + 8, right.y + 8, right.width - 16, 100), copy, 4, 4, fui::TextAlign::Center);
    if (!m.right && !failed)
      text(s, makeRect(right.x, right.y + 124, right.width, 100), tr(STR_MR_NOT_FIT_HELP), 4, 4,
           fui::TextAlign::Center);
    if (m.right) {
      std::snprintf(copy, sizeof(copy), m.wrongAccusations ? tr(STR_MR_RESULT_STATS) : tr(STR_MR_RESULT_ONE),
                    murdlesession::lives(m.wrongAccusations), m.wrongAccusations + 1);
      caps(s, makeRect(right.x, right.y + 122, right.width, 44), copy, 0, 2, fui::TextAlign::Center);
    }
    const int count = m.right ? 4 : failed ? 3 : 2, h = 50, y = right.bottom() - count * (h + 8) + 8;
    for (int n = 0; n < count; ++n) {
      const char* label;
      fui::ActionId action;
      if (m.right) {
        label = n == 0   ? (m.nextCase ? tr(STR_MURDLE_NEXT_CASE) : tr(STR_MR_CHOOSE_CASE))
                : n == 1 ? tr(STR_MR_REPLAY)
                : n == 2 ? tr(STR_MR_VIEW_DEDUCTION)
                         : tr(STR_MR_BACK_CASES);
        action = n == 0   ? (m.nextCase ? ActionDone : ActionCampaign)
                 : n == 1 ? ActionRetryCase
                 : n == 2 ? ActionKeepLooking
                          : ActionCampaign;
      } else if (failed) {
        label = n == 0 ? tr(STR_MR_RETRY_CASE) : n == 1 ? tr(STR_MR_VIEW_SOLUTION) : tr(STR_MR_BACK_CASES);
        action = n == 0 ? ActionRetryCase : n == 1 ? ActionSolution : ActionCampaign;
      } else {
        label = n == 0 ? tr(STR_MR_KEEP_INVESTIGATING) : tr(STR_MR_REVIEW_ACCUSATION);
        action = n == 0 ? ActionKeepLooking : ActionReviewAccusation;
      }
      const auto r = m.right ? makeRect(right.x + n % 2 * (right.width + 8) / 2, right.bottom() - 124 + n / 2 * 66,
                                        (right.width - 8) / 2, 58)
                             : makeRect(right.x, y + n * (h + 8), right.width, h);
      button(s, r, label, action, 0, n == 0, gamepanel::Mark::Play, !m.right);
    }
    return;
  }
  const int w = wide ? b.width / 2 - 12 : b.width, x = b.x;
  const int actions = m.right ? 252 : failed ? 190 : 126;
  caps(s, makeRect(x, b.y, w, 62),
       m.right  ? tr(STR_MR_RESOLVED)
       : failed ? tr(STR_MR_FAILED)
                : tr(STR_MR_NOT_FIT),
       2, 1, fui::TextAlign::Center);
  if (m.right)
    text(s, makeRect(x, b.y + 64, w, 32), tr(STR_MR_TRUTH), 4, 1, fui::TextAlign::Center);
  else {
    hearts(s, makeRect(x, b.y + 64, w, 50), murdlesession::lives(m.wrongAccusations), false);
    char label[64];
    std::snprintf(label, sizeof(label),
                  murdlesession::lives(m.wrongAccusations) == 1 ? tr(STR_MR_ONE_LIFE_LEFT) : tr(STR_MR_LIVES_LEFT),
                  murdlesession::lives(m.wrongAccusations));
    caps(s, makeRect(x, b.y + 116, w, 30), failed ? tr(STR_MR_NO_LIVES) : label, 0, 1, fui::TextAlign::Center);
  }
  const int artY = b.y + (m.right ? 102 : 154), textY = b.bottom() - actions - (m.right ? 130 : failed ? 100 : 182);
  verdictArt(s, makeRect(x + 10, artY, w - 20, std::max(50, textY - artY - 12)), m.right, failed);
  if (m.right || !failed) {
    dashboard::frame(s, makeRect(x, textY, w, m.right ? 90 : 100), 2);
    char copy[160];
    uint8_t correct[murdle::kMaxCats]{};
    for (int c = 0; c < m.puzzle->shape.cats; ++c) correct[c] = m.puzzle->assign[c][m.puzzle->murderRow];
    if (m.right)
      murdletext::accusationLine(*m.puzzle, correct, copy, sizeof(copy));
    else {
      AccuseModel a;
      a.puzzle = m.puzzle;
      std::memcpy(a.picks, m.picks, sizeof(a.picks));
      if (a.complete())
        murdletext::accusationLine(*m.puzzle, m.picks, copy, sizeof(copy));
      else
        std::snprintf(copy, sizeof(copy), "%s", tr(STR_MR_REVIEW_ACCUSATION));
    }
    if (!m.right)
      caps(s, makeRect(x + 8, textY + 4, w - 16, 28), tr(STR_MR_YOUR_ACCUSATION), 0, 1, fui::TextAlign::Center);
    text(s, makeRect(x + 10, textY + (m.right ? 4 : 36), w - 20, m.right ? 82 : 60), copy, 4, 3,
         fui::TextAlign::Center);
  }
  if (m.right) {
    char copy[80];
    std::snprintf(copy, sizeof(copy), m.wrongAccusations ? tr(STR_MR_RESULT_STATS) : tr(STR_MR_RESULT_ONE),
                  murdlesession::lives(m.wrongAccusations), m.wrongAccusations + 1);
    caps(s, makeRect(x, textY + 96, w, 30), copy, 0, 1, fui::TextAlign::Center);
  } else
    text(s, makeRect(x, textY + (failed ? 0 : 108), w, failed ? 90 : 60),
         failed ? tr(STR_MR_FAILED_HELP) : tr(STR_MR_NOT_FIT_HELP), 4, 3, fui::TextAlign::Center);
  const auto right =
      wide ? gamepanel::centeredBlock(makeRect(b.x + w + 24, b.y, b.width - w - 24, b.height), m.right  ? 240
                                                                                               : failed ? 178
                                                                                                        : 116)
           : makeRect(b.x, b.bottom() - (m.right ? 242 : 180), b.width, m.right ? 242 : 180);
  if (m.right) {
    button(s, makeRect(right.x, right.y, right.width, 54),
           m.nextCase ? tr(STR_MURDLE_NEXT_CASE) : tr(STR_MR_CHOOSE_CASE), m.nextCase ? ActionDone : ActionCampaign, 0,
           true, gamepanel::Mark::Play, true);
    button(s, makeRect(right.x, right.y + 62, right.width, 54), tr(STR_MR_REPLAY), ActionRetryCase, 0, false,
           gamepanel::Mark::Play, true);
    button(s, makeRect(right.x, right.y + 124, right.width, 54), tr(STR_MR_VIEW_DEDUCTION), ActionKeepLooking, 0, false,
           gamepanel::Mark::Grid, true);
    button(s, makeRect(right.x, right.y + 186, right.width, 54), tr(STR_MR_BACK_CASES), ActionCampaign, 0, false,
           gamepanel::Mark::Back, true);
  } else if (failed) {
    button(s, makeRect(right.x, right.y, right.width, 54), tr(STR_MR_RETRY_CASE), ActionRetryCase, 0, true,
           gamepanel::Mark::Play, true);
    button(s, makeRect(right.x, right.y + 62, right.width, 54), tr(STR_MR_VIEW_SOLUTION), ActionSolution, 0, false,
           gamepanel::Mark::Book, true);
    button(s, makeRect(right.x, right.y + 124, right.width, 54), tr(STR_MR_BACK_CASES), ActionCampaign, 0, false,
           gamepanel::Mark::Back, true);
  } else {
    button(s, makeRect(right.x, right.bottom() - 116, right.width, 54), tr(STR_MR_KEEP_INVESTIGATING),
           ActionKeepLooking, 0, true, gamepanel::Mark::Play, true);
    button(s, makeRect(right.x, right.bottom() - 54, right.width, 54), tr(STR_MR_REVIEW_ACCUSATION),
           ActionReviewAccusation, 0, false, gamepanel::Mark::Book, true);
  }
}
void buildConfirmNew(toybox::Screen& s) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  caps(s, makeRect(b.x, b.y + 40, b.width, 90), tr(STR_MR_REPLACE), 2, 2, fui::TextAlign::Center);
  const bool wide = b.width > b.height;
  text(s, makeRect(b.x + 12, b.y + 150, wide ? b.width / 2 - 24 : b.width - 24, 140), tr(STR_MR_REPLACE_HELP), 4, 5);
  button(s, makeRect(wide ? b.x + b.width / 2 + 8 : b.x, b.bottom() - 122, wide ? b.width / 2 - 8 : b.width, 54),
         tr(STR_MR_OPEN_CASE), ActionConfirmNew, 0, true);
  button(s, makeRect(wide ? b.x + b.width / 2 + 8 : b.x, b.bottom() - 54, wide ? b.width / 2 - 8 : b.width, 54),
         tr(STR_QUEST_BACK), ActionCancel);
}
void buildConfirmReset(toybox::Screen& s, int completed) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  const bool wide = b.width > b.height;
  char copy[80];
  std::snprintf(copy, sizeof(copy), tr(STR_MR_RESET_COUNT), completed, murdlecampaign::kCaseCount);
  if (wide) {
    const auto card = makeRect(b.x, b.y, b.width, b.height - 90);
    dashboard::frame(s, card, 2);
    caps(s, makeRect(card.x + 16, card.y + 12, card.width - 32, 48), tr(STR_MR_RESET_TITLE), 1, 1);
    text(s, makeRect(card.x + 16, card.y + 72, card.width - 32, 36), copy, 4);
    text(s, makeRect(card.x + 16, card.y + 124, card.width - 32, card.height - 136), tr(STR_MR_RESET_HELP), 4, 5);
    button(s, makeRect(b.x, b.bottom() - 62, (b.width - 12) / 2, 62), tr(STR_MR_KEEP_PROGRESS), ActionCancel, 0, true);
    button(s, makeRect(b.x + (b.width + 12) / 2, b.bottom() - 62, (b.width - 12) / 2, 62), tr(STR_MR_RESET_ALL),
           ActionConfirmReset);
    return;
  }
  const auto card = makeRect(b.x, b.y + 24, b.width, b.height - 190);
  dashboard::frame(s, card, 2);
  caps(s, makeRect(card.x + 16, card.y + 16, card.width - 32, 100), tr(STR_MR_RESET_TITLE), 2, 2);
  s.target().line({int16_t(card.x + 16), int16_t(card.y + 132)}, {int16_t(card.right() - 16), int16_t(card.y + 132)}, 1,
                  dashboard::ink);
  text(s, makeRect(card.x + 16, card.y + 152, card.width - 32, 84), copy, 4, 3);
  text(s, makeRect(card.x + 16, card.y + 248, card.width - 32, card.height - 264), tr(STR_MR_RESET_HELP), 4, 9);
  const auto actions = wide ? makeRect(card.right() + 24, b.y, b.right() - card.right() - 24, b.height)
                            : makeRect(b.x, b.bottom() - 138, b.width, 138);
  button(s, makeRect(actions.x, actions.bottom() - 138, actions.width, 62), tr(STR_MR_KEEP_PROGRESS), ActionCancel, 0,
         true);
  button(s, makeRect(actions.x, actions.bottom() - 62, actions.width, 62), tr(STR_MR_RESET_ALL), ActionConfirmReset);
}
namespace {
void helpDifficulties(toybox::Screen& s, fui::Rect b) {
  const bool wide = b.width > b.height;
  pixelui::text(s.target(), makeRect(b.x, b.y, b.width, 44), tr(STR_MR_HELP_DIFFICULTIES), toybox::kDisplayFont);
  char count[64];
  std::snprintf(count, sizeof(count), tr(STR_MR_HELP_TIER_CASES), murdlecampaign::kCasesPerTier);
  text(s, makeRect(b.x, b.y + 48, b.width, 30), count, 4);
  const auto area = makeRect(b.x, b.y + (wide ? 76 : 90), b.width, b.height - (wide ? 128 : 154));
  static constexpr StrId sizes[] = {StrId::STR_MR_HELP_BASIC_SIZE, StrId::STR_MR_HELP_NOSY_SIZE,
                                    StrId::STR_MR_HELP_MOTIVE_SIZE, StrId::STR_MR_HELP_MOTIVE_SIZE};
  static constexpr StrId rules[] = {StrId::STR_MR_HELP_BASIC_RULE, StrId::STR_MR_HELP_NOSY_RULE,
                                    StrId::STR_MR_HELP_HARD_RULE, StrId::STR_MR_HELP_IMPOSSIBLE_RULE};
  const int columns = wide ? 4 : 1, rows = wide ? 1 : 4;
  const int gap = 6, width = (area.width - (columns - 1) * gap) / columns;
  const int height = (area.height - (rows - 1) * gap) / rows;
  for (int n = 0; n < murdle::kTierCount; ++n) {
    const auto r =
        makeRect(area.x + (n % columns) * (width + gap), area.y + (n / columns) * (height + gap), width, height);
    dashboard::frame(s, r, 2);
    s.target().fill(makeRect(r.x + 8, r.y + 8, 34, 30), dashboard::ink);
    char number[8];
    std::snprintf(number, sizeof(number), "%d", n + 1);
    pixelui::text(s.target(), makeRect(r.x + 8, r.y + 6, 34, 34), number, toybox::kUiFont, fui::Color::White,
                  fui::TextAlign::Center);
    const auto heading = makeRect(r.x + 54, r.y + 6, r.width - 64, 34);
    pixelui::text(s.target(), heading, tierName(static_cast<murdle::Tier>(n)), toybox::kUiFont, fui::Color::Black);
    const int inset = 12;
    const int top = r.y + (wide ? 46 : 38), sizeH = wide ? 72 : 24;
    text(s, makeRect(r.x + inset, top, r.width - 2 * inset, sizeH), I18N.get(sizes[n]), 0, 3);
    const int ruleY = top + sizeH + 2;
    text(s, makeRect(r.x + inset, ruleY, r.width - 2 * inset, r.bottom() - ruleY - 8), I18N.get(rules[n]), 4, 5);
  }
}
}  // namespace
int howToPages(bool) { return 3; }
void buildHowTo(toybox::Screen& s, const HowToModel& m) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  const bool wide = b.width > b.height;
  const int pageIndex = std::clamp(m.page, 0, howToPages(wide) - 1);
  if (pageIndex == 2) {
    helpDifficulties(s, b);
    pageui::build(s, makeRect(b.x, b.bottom() - 48, b.width, 48), pageIndex + 1, howToPages(wide), ActionPage, -1,
                  ActionPage, 1);
    return;
  }
  const int w = wide ? b.width / 2 - 10 : b.width;
  caps(s, makeRect(b.x, b.y, w, 52), pageIndex ? tr(STR_MR_NOTEBOOK) : tr(STR_MR_SOLVE_CRIME), 2, 2);
  const auto area = wide ? makeRect(b.x + w + 20, b.y, b.width - w - 20, b.height - 64)
                         : makeRect(b.x, b.y + 66, b.width, b.height - 130);
  if (!pageIndex) {
    static constexpr StrId titles[] = {StrId::STR_MR_READ_CLUES, StrId::STR_MR_CROSS_DATA,
                                       StrId::STR_MR_MAKE_ACCUSATION};
    static constexpr StrId copy[] = {StrId::STR_MR_READ_HELP, StrId::STR_MR_CROSS_HELP, StrId::STR_MR_ACCUSE_HELP};
    const int h = wide ? area.height / 3 : (area.height - 70) / 3;
    for (int n = 0; n < 3; ++n) {
      auto r = makeRect(area.x, area.y + n * h, area.width, h);
      s.target().fill(makeRect(r.x, r.y + 4, 42, 42), dashboard::ink);
      char num[8];
      std::snprintf(num, sizeof(num), "%d.", n + 1);
      text(s, makeRect(r.x, r.y, 42, 44), num, 1, 1, fui::TextAlign::Center, true);
      caps(s, makeRect(r.x + 56, r.y, r.width - 56, 34), I18N.get(titles[n]), 1);
      text(s, makeRect(r.x + 56, r.y + 38, r.width - 56, wide ? 60 : 40), I18N.get(copy[n]), 4, wide ? 3 : 2);
      if (!wide) {
        const auto art = makeRect(r.x + 40, r.y + 84, r.width - 70, std::max(20, h - 94));
        if (n == 1) {
          const int side = std::min(64, int(art.height)), total = 3 * side + 72;
          const int x = art.x + (art.width - total) / 2, y = art.y + (art.height - side) / 2;
          murdleart::draw(s, makeRect(x, y, side, side), murdleart::Person15);
          murdleart::draw(s, makeRect(x + side + 36, y, side, side), murdleart::WeaponKnife);
          murdleart::draw(s, makeRect(x + 2 * (side + 36), y, side, side), murdleart::PlaceDock);
          for (int link = 0; link < 2; ++link)
            gamepanel::mark(s, makeRect(x + side + link * (side + 36) + 6, y + (side - 24) / 2, 24, 24),
                            gamepanel::Mark::Link);
        } else
          murdleart::draw(s, art, n == 0 ? murdleart::Read : murdleart::Accuse);
      }
      if (n < 2)
        s.target().line({r.x, int16_t(r.bottom() - 6)}, {int16_t(r.right()), int16_t(r.bottom() - 6)}, 1,
                        dashboard::ink);
    }
    const auto life =
        wide ? makeRect(b.x, b.bottom() - 132, w, 68) : makeRect(area.x, area.bottom() - 64, area.width, 60);
    dashboard::frame(s, life, 2);
    hearts(s, makeRect(life.x + 6, life.y + 4, life.width / 3, life.height - 8), 3, false);
    text(s, makeRect(life.x + life.width / 3 + 14, life.y + 4, life.width * 2 / 3 - 24, life.height - 8),
         tr(STR_MR_THREE_LIVES), 4, 3);
    if (wide) murdleart::draw(s, makeRect(b.x + 20, b.y + 60, w - 40, b.height - 206), murdleart::Menu);
  } else {
    const auto table = wide ? makeRect(b.x, b.y + 64, w, b.height - 132) : makeRect(area.x, area.y, area.width, 292);
    const int gutter = wide ? 100 : table.width * 2 / 5;
    const int cw = (table.width - gutter) / 3, hh = 56, rh = (table.height - hh) / 3;
    static constexpr StrId weapons[] = {StrId::STR_MURDLE_WEAPONS_NAME_0, StrId::STR_MURDLE_WEAPONS_NAME_9,
                                        StrId::STR_MURDLE_WEAPONS_NAME_8};
    static constexpr StrId names[] = {StrId::STR_MR_PERSON_22, StrId::STR_MR_PERSON_10, StrId::STR_MR_PERSON_2};
    const fui::BitmapRef* images[] = {&murdleart::Person15, &murdleart::Person00, &murdleart::Person01};
    static constexpr const fui::BitmapRef* weaponImages[] = {&murdleart::WeaponAxe, &murdleart::WeaponKnife,
                                                             &murdleart::WeaponJar};
    dashboard::frame(s, table, 2);
    for (int c = 0; c < 3; ++c) {
      auto r = makeRect(table.x + gutter + c * cw, table.y, cw, hh);
      dashboard::frame(s, r, 1);
      murdleart::draw(s, makeRect(r.x + 4, r.y + 3, r.width - 8, 30), *weaponImages[c]);
      caps(s, makeRect(r.x + 2, r.bottom() - 24, r.width - 4, 22), I18N.get(weapons[c]), 3, 1, fui::TextAlign::Center);
    }
    for (int r = 0; r < 3; ++r) {
      auto label = makeRect(table.x, table.y + hh + r * rh, gutter, rh);
      dashboard::frame(s, label, 1);
      murdleart::draw(s, makeRect(label.x + 3, label.y + 3, wide ? 32 : 54, label.height - 6), *images[r]);
      caps(s, makeRect(label.x + (wide ? 40 : 62), label.y + 4, label.width - (wide ? 44 : 66), label.height - 8),
           I18N.get(names[r]), 3, 2);
      for (int c = 0; c < 3; ++c) {
        auto box = makeRect(table.x + gutter + c * cw, label.y, cw, rh);
        dashboard::frame(s, box, 1);
        auto mark = makeRect(box.x + (box.width - 28) / 2, box.y + (box.height - 28) / 2, 28, 28);
        if ((r == 1 && c == 1) || (r == 2 && c == 2)) {
          lightui::surface(s.target(), box, dashboard::ink);
          check(s, mark, true);
        } else if (r == 0 && c == 1)
          dashboard::frame(s, mark, 2);
        else
          cross(s, mark);
      }
    }
    const int start = wide ? area.y + 8 : table.bottom() + 18, h = wide ? 54 : 50;
    for (int n = 0; n < 3; ++n) {
      auto r = makeRect(area.x, start + n * h, area.width, h);
      dashboard::frame(s, r, 1);
      auto mark = makeRect(r.x + 18, r.y + (r.height - 26) / 2, 26, 26);
      if (n == 0)
        dashboard::frame(s, mark, 2);
      else if (n == 1)
        cross(s, mark);
      else
        check(s, mark);
      static constexpr StrId labels[] = {StrId::STR_MR_EMPTY_MARK, StrId::STR_MR_NO_MARK, StrId::STR_MR_YES_MARK};
      caps(s, makeRect(r.x + 64, r.y + 3, r.width - 72, r.height - 6), I18N.get(labels[n]), 0, 2);
    }
    text(s, makeRect(area.x + 8, start + 3 * h + 16, area.width - 16, wide ? 60 : 48), tr(STR_MR_TAP_MARK), 4, 2);
    pixelui::text(s.target(), makeRect(area.x + 8, area.bottom() - 40, area.width - 16, 36), tr(STR_MR_NOTE_SAFE),
                  toybox::kSmallFont, fui::Color::Black, fui::TextAlign::Center);
  }
  pageui::build(s, makeRect(b.x, b.bottom() - 48, b.width, 48), pageIndex + 1, howToPages(wide), ActionPage, -1,
                ActionPage, 1);
}
void buildStorageError(toybox::Screen& s, bool saving) {
  const auto b = begin(s, tr(STR_REDESIGN_MURDLE));
  caps(s, makeRect(b.x, b.y + 36, b.width, 80), tr(STR_READING_STORAGE_ERROR), 2, 2);
  text(s, makeRect(b.x + 12, b.y + 140, b.width - 24, b.width > b.height ? 100 : 160),
       saving ? tr(STR_MR_SAVE_ERROR) : tr(STR_MR_LOAD_ERROR), 4, 5);
  button(s, makeRect(b.x, b.bottom() - 118, b.width, 54), tr(STR_RETRY), ActionRetry, 0, true);
  button(s, makeRect(b.x, b.bottom() - 54, b.width, 54), tr(STR_QUEST_BACK), ActionBack);
}
}  // namespace murdleui
