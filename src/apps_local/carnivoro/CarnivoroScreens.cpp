#include "CarnivoroScreens.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/MysteryArt.h"
#include "../ui/PageIndicator.h"
#include "../ui/ToyboxIcons.h"
#include "CarnivoroArt.h"

namespace carnivoroui {
namespace {
constexpr auto black = fui::Paint::solid(fui::Color::Black), white = fui::Paint::solid(fui::Color::White);
void text(fui::DrawTarget& t, fui::Rect b, const char* s, int font = 4, int lines = 1,
          fui::TextAlign align = fui::TextAlign::Left, fui::Color color = fui::Color::Black) {
  fittext::draw(t, b, s, font, lines, align, color);
}
void hit(Geometry& g, fui::Rect box, int action, int value = 0) {
  if (g.count < 24 && !box.empty()) g.hits[g.count++] = {box, action, value};
}
void icon(fui::DrawTarget& t, fui::Rect box, const freeink::Icon& mark, bool inverse = false) {
  t.bitmap(box, fui::bitmapFromIcon(mark), fui::BitmapMode::Contain, inverse ? white : black);
}
void caps(fui::DrawTarget& t, fui::Rect r, const char* label, int font = 1,
          fui::TextAlign align = fui::TextAlign::Left) {
  char copy[192];
  pixelui::copyCaps(copy, sizeof(copy), label);
  text(t, r, copy, font, 2, align);
}
void portrait(fui::DrawTarget& t, fui::Rect r, int n, bool inverse = false) {
  static constexpr const fui::BitmapRef* art[] = {&mysteryart::Panther, &mysteryart::Peacock, &mysteryart::Shark};
  if (n < 3)
    t.bitmap(r, *art[n], fui::BitmapMode::Contain, inverse ? white : black);
  else
    icon(t, r, *portraits[n], inverse);
}
void art(fui::DrawTarget& t, fui::Rect r, const fui::BitmapRef& a) { t.bitmap(r, a, fui::BitmapMode::Contain, black); }
void deductionExample(fui::DrawTarget& t, fui::Rect r) {
  caps(t, fui::makeRect(r.x, r.y, r.width, 30), tr(STR_CAR_GUIDE_DEDUCE_COUNTS), 0, fui::TextAlign::Center);
  const int gap = 10, w = (r.width - gap) / 2;
  static constexpr StrId names[] = {StrId::STR_CAR_GUIDE_DEDUCE_ANA, StrId::STR_CAR_GUIDE_DEDUCE_BRUNO};
  static constexpr StrId claims[] = {StrId::STR_CAR_GUIDE_DEDUCE_CLAIM0, StrId::STR_CAR_GUIDE_DEDUCE_CLAIM1};
  for (int n = 0; n < 2; ++n) {
    const auto box = fui::makeRect(r.x + n * (w + gap), r.y + 38, w, 74);
    t.stroke(box, black, 2);
    caps(t, fui::makeRect(box.x + 4, box.y + 4, w - 8, 26), I18N.get(names[n]), 0, fui::TextAlign::Center);
    text(t, fui::makeRect(box.x + 4, box.y + 34, w - 8, 34), I18N.get(claims[n]), 4, 2, fui::TextAlign::Center);
  }
  text(t, fui::makeRect(r.x, r.y + 122, r.width, r.height - 122), tr(STR_CAR_GUIDE_DEDUCE_RESULT), 4, 4);
}

void button(fui::DrawTarget& t, Geometry& g, fui::Rect box, const char* label, int action, int value = 0,
            bool primary = false, bool selected = false, const freeink::Icon* mark = nullptr, bool arrow = false) {
  char caption[192];
  pixelui::copyCaps(caption, sizeof(caption), label);
  label = caption;
  if (arrow)
    dashboard::action(t, box, label, mark, primary);
  else {
    t.fill(box, primary ? black : white);
    t.stroke(box, black, selected ? 4 : 2);
    if (box.width < 300) mark = nullptr;
    const int side = std::min(40, int(box.height) - 12);
    if (mark) icon(t, fui::makeRect(box.x + 16, box.y + (box.height - side) / 2, side, side), *mark, primary);
    text(t,
         fui::makeRect(box.x + (mark ? side + 30 : 6), box.y + 3, box.width - (mark ? side + 46 : 12), box.height - 6),
         label, box.height >= 48 ? 1 : 0, box.height >= 48 ? 2 : 1, fui::TextAlign::Center,
         primary ? fui::Color::White : fui::Color::Black);
  }
  hit(g, box, action, value);
}
void noteCell(fui::DrawTarget& t, Geometry& g, fui::Rect box, const char* label, int action = fui::NO_ACTION,
              int value = 0, bool selected = false) {
  t.fill(box, selected ? black : white);
  t.stroke(box, black, 2);
  text(t, box.inset({3, 6, 3, 6}), label, pixelui::kSmall, 1, fui::TextAlign::Center,
       selected ? fui::Color::White : fui::Color::Black);
  if (action != fui::NO_ACTION) hit(g, box, action, value);
}
void pager(fui::DrawTarget& t, Geometry& g, fui::Rect band, int page, int pages) {
  const auto nav = pageui::draw(t, band, page + 1, pages);
  if (page > 0) hit(g, nav.before, Previous);
  if (page + 1 < pages) hit(g, nav.after, Next);
}
void checkbox(fui::DrawTarget& t, fui::Rect box, bool checked, bool inverse = false) {
  t.stroke(box, inverse ? white : black, 2);
  if (!checked) return;
  t.fill(box, inverse ? white : black);
  icon(t, box.inset(fui::Insets{4, 4, 4, 4}), extra_check_32, !inverse);
}
const char* diet(int i) {
  return i == 0 || i == 2 ? tr(STR_CAR_CARNIVORE) : i >= 5 ? tr(STR_CAR_HERBIVORE) : tr(STR_CAR_OMNIVORE);
}
void portraitRow(fui::DrawTarget& t, fui::Rect box, int first, int count) {
  const int width = box.width / count, side = std::min(width - 12, box.height - 30);
  for (int n = 0; n < count; ++n) {
    const int i = first + n;
    portrait(t, fui::makeRect(box.x + n * width + (width - side) / 2, box.y, side, side), i);
    text(t, fui::makeRect(box.x + n * width, box.bottom() - 28, width, 28), animalName(i), 0, 1,
         fui::TextAlign::Center);
  }
}
void rangeLabel(char* out, size_t size, int first, int last) {
  if (first == last)
    std::snprintf(out, size, "%d", first);
  else
    std::snprintf(out, size, "%d-%d", first, last);
}
void difficultyGuide(fui::DrawTarget& t, Geometry& g, fui::Rect body, Model& m, bool wide) {
  caps(t, fui::makeRect(body.x, body.y, body.width, 38), tr(STR_MR_HELP_DIFFICULTIES), 1);
  static constexpr StrId hints[] = {StrId::STR_CAR_TIER_HINT0, StrId::STR_CAR_TIER_HINT1, StrId::STR_CAR_TIER_HINT2,
                                    StrId::STR_CAR_TIER_HINT3};
  const int gap = 8, count = wide ? 2 : 4, first = wide ? (m.page - 8) * 2 : 0;
  const int height = wide ? body.height - 112 : (body.height - 112 - 3 * gap) / 4;
  const int width = wide ? (body.width - gap) / 2 : body.width;
  for (int n = 0; n < count; ++n) {
    const auto tier = static_cast<carnivoro::Difficulty>(first + n);
    const auto& rules = carnivoro::rules(tier);
    const auto box = fui::makeRect(body.x + (wide ? n * (width + gap) : 0),
                                   body.y + 48 + (wide ? 0 : n * (height + gap)), width, height);
    t.stroke(box, black, 2);
    std::snprintf(m.formatted, sizeof(m.formatted), "%d %s", first + n + 1, difficultyName(tier));
    caps(t, fui::makeRect(box.x + 8, box.y + 4, box.width - 16, 30), m.formatted, 1);
    std::snprintf(m.formatted, sizeof(m.formatted),
                  carnivoro::lives(tier) == 1 ? tr(STR_CAR_TIER_COUNT_ONE) : tr(STR_CAR_TIER_COUNT_MANY),
                  carnivoro::caseCount(tier));
    text(t, fui::makeRect(box.x + 8, box.y + 36, box.width - 16, 22), m.formatted, 3);
    char animals[12], liars[12], culprits[12];
    rangeLabel(animals, sizeof(animals), rules.minAnimals, rules.maxAnimals);
    rangeLabel(liars, sizeof(liars), rules.minLiars, rules.maxLiars);
    rangeLabel(culprits, sizeof(culprits), rules.minCulprits, rules.maxCulprits);
    std::snprintf(m.formatted, sizeof(m.formatted), tr(STR_CAR_TIER_RULES), animals, liars, culprits);
    text(t, fui::makeRect(box.x + 8, box.y + 60, box.width - 16, wide ? 74 : 44), m.formatted, 3, wide ? 3 : 2);
    text(t, fui::makeRect(box.x + 8, box.y + (wide ? 146 : 106), box.width - 16, box.height - (wide ? 154 : 110)),
         I18N.get(hints[first + n]), 3, wide ? 4 : 1);
  }
  pager(t, g, fui::makeRect(body.x, body.bottom() - 48, body.width, 48), m.page, m.textPages);
}
const char* truthLabel(carnivoro::Note note) {
  static constexpr StrId labels[] = {StrId::STR_CAR_UNKNOWN_SHORT, StrId::STR_CAR_TRUTH, StrId::STR_CAR_LIE};
  return I18N.get(labels[static_cast<int>(note)]);
}
const char* guiltLabel(carnivoro::GuiltNote note) {
  static constexpr StrId labels[] = {StrId::STR_CAR_UNKNOWN_SHORT, StrId::STR_CAR_SUSPECT_SHORT,
                                     StrId::STR_CAR_INNOCENT_NOTE};
  return I18N.get(labels[static_cast<int>(note)]);
}
void analysis(fui::DrawTarget& t, Geometry& g, fui::Rect band, const carnivoro::State& state, int suspect) {
  t.line({band.x, band.y}, {int16_t(band.right()), band.y}, 1, black);
  const int small = (band.width - 26) * 21 / 100, gap = 6, top = band.y + 4;
  for (int n = 0; n < 3; ++n)
    button(t, g, fui::makeRect(band.x + 8 + n * (small + gap), top, small, 32),
           truthLabel(static_cast<carnivoro::Note>(n)), Note, suspect * 3 + n, int(state.notes[suspect]) == n);
  const int x = band.x + 8 + 3 * (small + gap);
  const auto guilt = carnivoro::guiltNote(state, suspect);
  const auto mark = fui::makeRect(x, top, band.right() - x - 8, 32);
  checkbox(t, fui::makeRect(x + 2, top + 2, 28, 28), guilt != carnivoro::GuiltNote::Unknown);
  text(t, fui::makeRect(x + 36, top, mark.width - 36, 32),
       guilt == carnivoro::GuiltNote::Innocent ? tr(STR_CAR_INNOCENT_NOTE) : tr(STR_CAR_SUSPECT_SHORT), 3);
  hit(g, mark, CycleGuilt, suspect);
}
int statementHeight(fui::DrawTarget& t, const carnivoro::Case& c, Model& m, int width, int suspect, bool reveal,
                    bool compact = false) {
  statement(c, suspect, m.formatted, sizeof(m.formatted));
  fui::TextStyle style;
  style.font = 4;
  style.maxLines = 16;
  int lines = 0;
  fui::layoutText(t, fui::makeRect(0, 0, width - 140, 1000), m.formatted, style,
                  [&](const char*, fui::Rect) { ++lines; });
  return std::max(compact ? 88 : 104, (compact ? 32 : 40) + lines * t.lineHeight(4) + (compact ? 4 : 8)) +
         (reveal ? 34 : 42);
}
int statementCapacity(fui::DrawTarget& t, const carnivoro::Case& c, Model& m, int width, int height, bool reveal,
                      bool compact = false) {
  for (int capacity = 3; capacity > 1; --capacity) {
    bool fits = true;
    for (int first = 0; first < c.count; first += capacity) {
      int required = 0;
      for (int i = first; i < std::min(first + capacity, int(c.count)); ++i)
        required += statementHeight(t, c, m, width, i, reveal, compact) + (i == first ? 0 : 8);
      if (required > height) fits = false;
    }
    if (fits) return capacity;
  }
  return 1;
}
void overview(fui::DrawTarget& t, Geometry& g, fui::Rect box, const carnivoro::Case& c, const carnivoro::State& state,
              const carnivoro::Solution* solution) {
  const bool wide = box.width > box.height;
  if (!wide) text(t, fui::makeRect(box.x, box.y, box.width, 30), tr(STR_CAR_OVERVIEW_HINT), 4);
  const int headingY = box.y + (wide ? 0 : 34);
  const int columns = wide ? 2 : 1, columnW = (box.width - (columns - 1) * 12) / columns;
  for (int col = 0; col < columns; ++col) {
    const int x = box.x + col * (columnW + 12), nameW = columnW * (wide ? 36 : 40) / 100, truthW = columnW * 27 / 100;
    text(t, fui::makeRect(x, headingY, nameW, 24), tr(STR_CAR_SUSPECT_SHORT), 3);
    text(t, fui::makeRect(x + nameW + 6, headingY, truthW, 24), tr(STR_CAR_SINCERITY_NOTE), 3);
    text(t, fui::makeRect(x + nameW + truthW + 12, headingY, columnW - nameW - truthW - 12, 24), tr(STR_CAR_GUILT_NOTE),
         3);
  }
  const int headingH = wide ? 28 : 64;
  const int top = box.y + headingH, height = box.height - headingH;
  for (int i = 0; i < c.count; ++i) {
    const int rows = (c.count + columns - 1) / columns, rowH = height / rows;
    const auto row = fui::makeRect(box.x + (i / rows) * (columnW + 12), top + (i % rows) * rowH, columnW, rowH - 6);
    const int nameW = row.width * (wide ? 36 : 40) / 100, truthW = row.width * 27 / 100;
    const auto name = fui::makeRect(row.x, row.y, nameW, row.height);
    const auto truth = fui::makeRect(name.right() + 6, row.y, truthW, row.height);
    const auto guilt = fui::makeRect(truth.right() + 6, row.y, row.right() - truth.right() - 6, row.height);
    t.stroke(name, black, 2);
    const int side = wide ? 0 : std::min(36, name.height - 8);
    if (side) portrait(t, fui::makeRect(name.x + 4, name.y + (name.height - side) / 2, side, side), i);
    text(t,
         fui::makeRect(name.x + (side ? side + 8 : 6), name.y + 3, name.width - (side ? side + 12 : 12),
                       name.height - 6),
         animalName(i), 0, 2, fui::TextAlign::Center);
    hit(g, name, ReadStatement, i);
    if (solution) {
      noteCell(t, g, truth, truthLabel((solution->truth >> i) & 1 ? carnivoro::Note::Truth : carnivoro::Note::Lie));
      noteCell(t, g, guilt, (solution->guilty >> i) & 1 ? tr(STR_CAR_CULPRIT_BADGE) : tr(STR_CAR_INNOCENT_NOTE));
    } else {
      noteCell(t, g, truth, truthLabel(state.notes[i]), CycleNote, i, state.notes[i] != carnivoro::Note::Unknown);
      const auto note = carnivoro::guiltNote(state, i);
      noteCell(t, g, guilt, guiltLabel(note), CycleGuilt, i, note != carnivoro::GuiltNote::Unknown);
    }
  }
}
void statementCard(fui::DrawTarget& t, Geometry& g, Model& m, const carnivoro::Case& c, fui::Rect box, int suspect,
                   bool reveal, bool compact = false) {
  const auto& state = *m.state;
  t.stroke(box, black, 2);
  const int bodyH = box.height - (reveal ? 34 : 42), side = std::min(80, std::max(32, bodyH - 24));
  portrait(t, fui::makeRect(box.x + 6 + (116 - side) / 2, box.y + 4, side, side), suspect);
  const int x = box.x + 130, width = box.right() - x - 10;
  text(t, fui::makeRect(x, box.y + 2, width - 66, compact ? 28 : 34), animalName(suspect), compact ? 0 : 1);
  static constexpr int heights[] = {180, 165, 195, 172, 205, 188, 198};
  std::snprintf(m.formatted, sizeof(m.formatted), "%d cm", heights[suspect]);
  text(t, fui::makeRect(box.right() - 66, box.y + 5, 58, 21), m.formatted, 3, 1, fui::TextAlign::Right);
  text(t, fui::makeRect(box.x + 3, box.y + bodyH - 22, 120, 22), diet(suspect), 3, 1, fui::TextAlign::Center);
  statement(c, suspect, m.formatted, sizeof(m.formatted));
  text(t, fui::makeRect(x, box.y + (compact ? 32 : 40), width, bodyH - (compact ? 32 : 40)), m.formatted, 4, 6);
  if (reveal) {
    const auto label = fui::makeRect(x, box.bottom() - 30, std::min(116, width), 26);
    t.stroke(label, black, 2);
    text(t, label, (m.solution.truth >> suspect) & 1 ? tr(STR_CAR_TRUTH) : tr(STR_CAR_LIE), 0, 1,
         fui::TextAlign::Center);
    if ((m.solution.guilty >> suspect) & 1) {
      const auto guilty = fui::makeRect(label.right() + 8, label.y, width - label.width - 8, 26);
      t.fill(guilty, black);
      text(t, guilty, tr(STR_CAR_CULPRIT_BADGE), 0, 1, fui::TextAlign::Center, fui::Color::White);
    }
  } else
    analysis(t, g, fui::makeRect(box.x, box.y + bodyH, box.width, 42), state, suspect);
}
}  // namespace
const char* animalName(int i) {
  static constexpr StrId names[] = {StrId::STR_CAR_PANTHER, StrId::STR_CAR_PEACOCK, StrId::STR_CAR_SHARK,
                                    StrId::STR_CAR_FOX,     StrId::STR_CAR_BEAR,    StrId::STR_CAR_STAG,
                                    StrId::STR_CAR_HORSE};
  return i >= 0 && i < 7 ? I18N.get(names[i]) : "";
}
const char* difficultyName(carnivoro::Difficulty d) {
  static constexpr StrId names[] = {StrId::STR_CAR_EASY, StrId::STR_CAR_NORMAL, StrId::STR_CAR_ADVANCED,
                                    StrId::STR_CAR_EXPERT};
  return I18N.get(names[static_cast<int>(d)]);
}
void statement(const carnivoro::Case& c, int i, char* out, size_t cap) {
  static constexpr StrId formats[] = {
      StrId::STR_CAR_GUILTY,          StrId::STR_CAR_INNOCENT,        StrId::STR_CAR_EITHER, StrId::STR_CAR_BOTH,
      StrId::STR_CAR_SAYS_TRUTH,      StrId::STR_CAR_SAYS_LIE,        StrId::STR_CAR_SAME,   StrId::STR_CAR_DIFFERENT,
      StrId::STR_CAR_CLAIM_CARNIVORE, StrId::STR_CAR_CLAIM_HERBIVORE, StrId::STR_CAR_TALLER, StrId::STR_CAR_SHORTER,
      StrId::STR_CAR_NEIGHBOUR};
  const auto& s = c.statements[i];
  // Circular adjacency is a rule of the case, not the current page or card layout.
  const int a = s.kind == carnivoro::Claim::Neighbour ? (i + c.count - 1) % c.count : s.a;
  const int b = s.kind == carnivoro::Claim::Neighbour ? (i + 1) % c.count : s.b;
  std::snprintf(out, cap, I18N.get(formats[static_cast<int>(s.kind)]), animalName(a), animalName(b));
}
void draw(fui::DrawTarget& t, const fui::DeviceContext& device, Model& m, Geometry& g, pagedtext::Work&) {
  g = {};
  const auto body = pixelui::geometry(device, 8).body;
  const auto& state = *m.state;
  const bool wide = body.width > body.height;
  g.caseCapacity = wide ? 8 : 16;
  g.suspectCapacity = wide ? 1 : 3;
  if (m.saveError) {
    text(t, fui::makeRect(body.x + 12, body.y + 20, body.width - 24, body.height - 106), tr(STR_CAR_SAVE_ERROR), 4, 10);
    button(t, g, fui::makeRect(body.x, body.bottom() - 64, body.width, 64), tr(STR_CAR_SAVE_RETRY), RetrySave, 0, true);
    return;
  }
  if (m.view == View::Menu) {
    const int row = wide ? 54 : 64, gap = 8;
    const auto hero = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 10, body.height)
                           : fui::makeRect(body.x, body.y, body.width, body.height - 4 * (row + gap) - 8);
    t.stroke(hero, black, 2);
    text(t, fui::makeRect(hero.x + 12, hero.y + 12, hero.width - 24, 30), tr(STR_CAR_DEDUCTION), 0);
    text(t, fui::makeRect(hero.x + 8, hero.y + 46, hero.width - 16, 58), tr(STR_CAR_WHO), 2, 1, fui::TextAlign::Center);
    portraitRow(t, fui::makeRect(hero.x + 8, hero.y + 116, hero.width - 16, std::max(64, hero.height - 184)), 0, 3);
    text(t, fui::makeRect(hero.x + 12, hero.bottom() - 55, hero.width - 24, 48), tr(STR_CAR_TAGLINE), 4, 2,
         fui::TextAlign::Center);
    const int x = wide ? hero.right() + 20 : body.x, width = wide ? body.right() - x : body.width;
    const int top = wide ? body.y + (body.height - 4 * (row + gap)) / 2 : hero.bottom() + 16;
    const int actions[] = {Open, Campaign, Help, ResetAll};
    const char* labels[] = {tr(STR_CAR_OPEN), tr(STR_CAR_CAMPAIGN), tr(STR_CAR_HELP), tr(STR_CAR_RESET_ALL)};
    const freeink::Icon* marks[] = {&extra_notes_32, &extra_route_32, &extra_information_32, &icon_go_trash_32};
    for (int n = 0; n < 4; ++n)
      button(t, g, fui::makeRect(x, top + n * (row + gap), width, row), labels[n], actions[n], 0, n == m.focus, false,
             marks[n], true);
    return;
  }
  if (m.view == View::ResetConfirm) {
    text(t, fui::makeRect(body.x + 8, body.y + 12, body.width - 16, 72), tr(STR_CAR_RESET_TITLE), 1, 2,
         fui::TextAlign::Center);
    text(t, fui::makeRect(body.x + 16, body.y + 104, body.width - 32, body.height - 264), tr(STR_CAR_RESET_HELP), 4, 8);
    button(t, g, fui::makeRect(body.x, body.bottom() - 132, body.width, 56), tr(STR_CAR_CANCEL), Back);
    button(t, g, fui::makeRect(body.x, body.bottom() - 64, body.width, 64), tr(STR_CAR_RESET_ALL), ConfirmReset, 0,
           true, false, &icon_go_trash_32);
    return;
  }
  if (m.view == View::Help) {
    m.textPages = wide ? 10 : 5;
    m.page = std::clamp(m.page, 0, m.textPages - 1);
    if (m.page >= (wide ? 8 : 4)) {
      difficultyGuide(t, g, body, m, wide);
      return;
    }
    const int guidePage = wide ? m.page / 2 : m.page;
    static constexpr StrId titles[] = {StrId::STR_CAR_GUIDE_TITLE0, StrId::STR_CAR_GUIDE_TITLE1,
                                       StrId::STR_CAR_GUIDE_TITLE2, StrId::STR_CAR_GUIDE_TITLE3};
    static constexpr StrId heads[][4] = {
        {StrId::STR_CAR_GUIDE_SOLVE, StrId::STR_CAR_GUIDE_SINCERE, StrId::STR_CAR_GUIDE_EXAMPLE, StrId::STR_NONE_OPT},
        {StrId::STR_CAR_GUIDE_OR, StrId::STR_CAR_GUIDE_AND, StrId::STR_CAR_GUIDE_COMPARE,
         StrId::STR_CAR_GUIDE_TRUTH_EXAMPLE},
        {StrId::STR_CAR_GUIDE_DEDUCTION, StrId::STR_CAR_GUIDE_HEIGHT, StrId::STR_CAR_GUIDE_MARKS, StrId::STR_NONE_OPT},
        {StrId::STR_CAR_GUIDE_CONFIRM, StrId::STR_CAR_GUIDE_LIVES, StrId::STR_CAR_GUIDE_CAMPAIGN,
         StrId::STR_CAR_GUIDE_REPEAT}};
    static constexpr StrId copy[][4] = {{StrId::STR_CAR_GUIDE_SOLVE_COPY, StrId::STR_CAR_GUIDE_SINCERE_COPY,
                                         StrId::STR_CAR_GUIDE_EXAMPLE_COPY, StrId::STR_NONE_OPT},
                                        {StrId::STR_CAR_GUIDE_OR_COPY, StrId::STR_CAR_GUIDE_AND_COPY,
                                         StrId::STR_CAR_GUIDE_COMPARE_COPY, StrId::STR_CAR_GUIDE_TRUTH_EXAMPLE_COPY},
                                        {StrId::STR_CAR_GUIDE_DEDUCE_RESULT, StrId::STR_CAR_GUIDE_HEIGHT_COPY,
                                         StrId::STR_CAR_GUIDE_MARKS_COPY, StrId::STR_NONE_OPT},
                                        {StrId::STR_CAR_GUIDE_CONFIRM_COPY, StrId::STR_CAR_GUIDE_LIVES_COPY,
                                         StrId::STR_CAR_GUIDE_CAMPAIGN_COPY, StrId::STR_CAR_GUIDE_REPEAT_COPY}};
    static constexpr const fui::BitmapRef* icons[][4] = {
        {&mysteryart::Document, &mysteryart::Masks, &mysteryart::Bulb, nullptr},
        {&mysteryart::Or, &mysteryart::And, &mysteryart::Sincerity, &mysteryart::Neighbours},
        {&mysteryart::Bulb, &mysteryart::HeightDiet, &mysteryart::Marks, nullptr},
        {&mysteryart::Confirm, &mysteryart::Lives, &mysteryart::Campaign, &mysteryart::RepeatSave}};
    if (wide) {
      caps(t, fui::makeRect(body.x, body.y, 150, 38), tr(STR_CAR_HELP), 0);
      caps(t, fui::makeRect(body.x + 158, body.y, body.width - 158, 38), I18N.get(titles[guidePage]), 1);
      const int width = (body.width - 24) / 2;
      for (int col = 0; col < 2; ++col) {
        const int n = (m.page % 2) * 2 + col;
        if (!icons[guidePage][n]) continue;
        const int x = body.x + col * (width + 24), y = body.y + 54;
        art(t, fui::makeRect(x, y, 32, 32), *icons[guidePage][n]);
        caps(t, fui::makeRect(x + 44, y, width - 44, 32), I18N.get(heads[guidePage][n]), 0);
        if (guidePage == 2 && n == 0)
          deductionExample(t, fui::makeRect(x, y + 42, width, body.height - 160));
        else
          text(t, fui::makeRect(x, y + 44, width, body.height - 164), I18N.get(copy[guidePage][n]), 4, 16);
      }
      pager(t, g, fui::makeRect(body.x, body.bottom() - 44, body.width, 44), m.page, m.textPages);
      return;
    }
    caps(t, fui::makeRect(body.x, body.y, body.width, 30), tr(STR_CAR_HELP), 0);
    caps(t, fui::makeRect(body.x, body.y + 32, body.width, 42), I18N.get(titles[guidePage]), 1);
    int required[4] = {}, total = 0, count = 0;
    const int iconW = 78, width = body.width - iconW - 8;
    for (int n = 0; n < 4 && icons[guidePage][n]; ++n) {
      fui::TextStyle style;
      style.font = 4;
      style.maxLines = 16;
      int lines = 0;
      fui::layoutText(t, fui::makeRect(0, 0, width, 1000), I18N.get(copy[guidePage][n]), style,
                      [&](const char*, fui::Rect) { ++lines; });
      required[n] = guidePage == 2 && n == 0 ? 236 : 30 + lines * t.lineHeight(4) + 8;
      total += required[n];
      ++count;
    }
    const int extra = std::max(0, int(body.height) - 144 - total) / std::max(1, count);
    int y = body.y + 80;
    for (int n = 0; n < count; ++n) {
      const int h = required[n] + extra;
      if (guidePage == 2 && n == 0) {
        caps(t, fui::makeRect(body.x, y, body.width, 30), I18N.get(heads[guidePage][n]), 0);
        deductionExample(t, fui::makeRect(body.x + 8, y + 36, body.width - 16, h - 44));
      } else {
        art(t, fui::makeRect(body.x + 4, y + 8, iconW - 12, std::min(h - 20, 100)), *icons[guidePage][n]);
        const int x = body.x + iconW + 8;
        caps(t, fui::makeRect(x, y, width, 30), I18N.get(heads[guidePage][n]), 0);
        text(t, fui::makeRect(x, y + 30, width, h - 38), I18N.get(copy[guidePage][n]), 4, 16);
      }
      y += h;
    }
    pager(t, g, fui::makeRect(body.x, body.bottom() - 44, body.width, 44), m.page, m.textPages);
    return;
  }
  if (m.view == View::Cases) {
    const int count = carnivoro::caseCount(state.difficulty), pages = (count + g.caseCapacity - 1) / g.caseCapacity;
    m.casePage = std::clamp(m.casePage, 0, pages - 1);
    const auto side = wide ? fui::makeRect(body.x, body.y, 220, body.height) : body;
    caps(t, fui::makeRect(side.x, side.y, wide ? side.width : side.width - 148, 48), tr(STR_CAR_SELECT_CASE), 0,
         fui::TextAlign::Left);
    button(t, g,
           wide ? fui::makeRect(side.x, side.bottom() - 48, side.width, 48)
                : fui::makeRect(side.right() - 140, side.y, 140, 48),
           tr(STR_MR_RESET), ResetAll);
    int solved = 0;
    for (const auto status : state.status)
      if (status == carnivoro::Status::Solved) ++solved;
    const int progressY = body.y + (wide ? 48 : 60);
    caps(t, fui::makeRect(side.x, progressY, side.width / 2, 34), tr(STR_MR_SOLVED), 0);
    std::snprintf(m.formatted, sizeof(m.formatted), "%d / %d", solved, carnivoro::kCases);
    text(t, fui::makeRect(side.x + side.width / 2, progressY, side.width / 2, 34), m.formatted, 0, 1,
         fui::TextAlign::Right);
    const auto bar = fui::makeRect(side.x, progressY + 38, side.width, 12);
    t.stroke(bar, black, 2);
    if (solved) t.fill(fui::makeRect(bar.x + 3, bar.y + 3, (bar.width - 6) * solved / carnivoro::kCases, 6), black);
    caps(t, fui::makeRect(side.x, body.y + (wide ? 100 : 120), side.width, 24), tr(STR_UI_DIFFICULTY_9106E9), 3);
    const int columns = wide ? 1 : 2, gap = 8;
    for (int n = 0; n < 4; ++n) {
      const auto box = fui::makeRect(side.x + n % columns * (side.width + gap) / columns,
                                     body.y + (wide ? 128 : 150) + n / columns * (wide ? 48 : 52),
                                     (side.width - gap * (columns - 1)) / columns, 44);
      button(t, g, box, difficultyName(static_cast<carnivoro::Difficulty>(n)), SelectDifficulty, n,
             m.keyboardFocus ? m.focusAction == SelectDifficulty && m.focusValue == n
                             : n == static_cast<int>(state.difficulty));
    }
    const auto area = wide ? fui::makeRect(side.right() + 16, body.y, body.width - side.width - 16, body.height)
                           : fui::makeRect(body.x, body.y + 260, body.width, body.height - 260);
    std::snprintf(m.formatted, sizeof(m.formatted), tr(STR_CAR_TIER_CASE_COUNT), count);
    caps(t, fui::makeRect(area.x, area.y, area.width, 32), m.formatted, 0);
    const int rows = g.caseCapacity / 4, width = (area.width - 3 * gap) / 4;
    const int height = (area.height - 96 - (rows - 1) * gap) / rows;
    for (int n = 0; n < g.caseCapacity; ++n) {
      const int id = carnivoro::caseAt(state.difficulty, m.casePage * g.caseCapacity + n);
      if (id < 0) break;
      const auto box =
          fui::makeRect(area.x + n % 4 * (width + gap), area.y + 40 + n / 4 * (height + gap), width, height);
      const auto status = carnivoro::caseStatus(state, id);
      const bool playing = state.caseId == id && status == carnivoro::Status::Active;
      const bool inverse = m.keyboardFocus ? m.focusAction == ChooseCase && m.focusValue == id : playing;
      t.fill(box, inverse ? black : white);
      t.stroke(box, black, 2);
      std::snprintf(m.formatted, sizeof(m.formatted), "%02d", carnivoro::caseNumber(id));
      text(t, fui::makeRect(box.x + 4, box.y + 2, box.width - 8, box.height - 48), m.formatted, wide ? 0 : 1, 1,
           fui::TextAlign::Center, inverse ? fui::Color::White : fui::Color::Black);
      const auto label = fui::makeRect(box.x + 4, box.bottom() - 46, box.width - 8, 44);
      const bool solvedCase = state.status[id] == carnivoro::Status::Solved;
      if (solvedCase) t.fill(label, black);
      const char* caption = solvedCase                               ? tr(STR_MR_SOLVED_CASE)
                            : status == carnivoro::Status::Exhausted ? tr(STR_CAR_EXHAUSTED)
                            : playing                                ? tr(STR_MR_IN_PROGRESS)
                                                                     : tr(STR_MR_UNSOLVED);
      char copy[64];
      pixelui::copyCaps(copy, sizeof(copy), caption);
      text(t, label, copy, 3, 2, fui::TextAlign::Center, solvedCase || inverse ? fui::Color::White : fui::Color::Black);
      hit(g, box, ChooseCase, id);
    }
    pager(t, g, fui::makeRect(area.x, body.bottom() - 48, area.width, 48), m.casePage, pages);
    return;
  }
  const auto* c = carnivoro::getCase(state.caseId);
  if (!c) return;
  if (m.view == View::Accuse) g.suspectCapacity = wide ? 3 : c->count;
  const bool closed = carnivoro::closed(state, state.caseId), reveal = m.view == View::Solution && closed;
  const bool explainSolution = reveal && c->count == 3 && c->culprits == 1 && c->lies == 1;
  const bool solvedResult =
      m.view == View::Verdict && carnivoro::caseStatus(state, state.caseId) == carnivoro::Status::Solved;
  if (!solvedResult) {
    std::snprintf(m.formatted, sizeof(m.formatted), tr(STR_CAR_CASE), carnivoro::caseNumber(state.caseId));
    text(t, fui::makeRect(body.x, body.y, body.width * 2 / 5, 32), m.formatted, 1);
    std::snprintf(m.formatted, sizeof(m.formatted),
                  carnivoro::remaining(state, state.caseId) == 1 ? tr(STR_CAR_LIFE_ONE) : tr(STR_CAR_LIVES),
                  carnivoro::remaining(state, state.caseId));
    text(t, fui::makeRect(body.x + body.width * 2 / 5, body.y, body.width * 3 / 5, 32),
         (m.view == View::Solution || (m.view == View::Overview && closed)) ? tr(STR_CAR_SOLUTION)
         : m.view == View::Verdict && !closed                               ? tr(STR_CAR_RESULT_WRONG_BADGE)
                                                                            : m.formatted,
         0, 1, fui::TextAlign::Right);
  }
  const bool rules = m.view == View::Statements || m.view == View::Overview;
  if (rules) {
    std::snprintf(m.formatted, sizeof(m.formatted), tr(STR_CAR_RULES), c->count, c->culprits,
                  c->culprits == 1 ? tr(STR_CAR_CULPRIT_ONE) : tr(STR_CAR_CULPRIT_MANY), c->lies,
                  c->lies == 1 ? tr(STR_CAR_LIE_ONE) : tr(STR_CAR_LIE_MANY));
    text(t, fui::makeRect(body.x, body.y + 34, body.width, 32), m.formatted, 4);
  }
  const int reserve = solvedResult ? 0 : rules ? 72 : 42;
  const auto content = fui::makeRect(body.x, body.y + reserve, body.width, body.height - reserve);
  if (m.view == View::Overview) {
    const auto list = fui::makeRect(content.x, content.y, content.width, content.height - 64);
    overview(t, g, list, *c, state, closed ? &m.solution : nullptr);
    button(t, g, fui::makeRect(content.x, content.bottom() - 58, content.width, 58),
           closed ? tr(STR_CAR_VIEW_SOLUTION) : tr(STR_CAR_ACCUSE), closed ? Back : Accuse, 0, true, false,
           &extra_carnivoro_gavel_32);
    return;
  }
  if (m.view == View::Statements || m.view == View::Solution) {
    const int footer = reveal ? (explainSolution ? 224 : 160) : 124;
    g.suspectCapacity =
        statementCapacity(t, *c, m, content.width - (wide ? 244 : 0),
                          content.height - (wide ? 0 : footer + (explainSolution ? 10 : 20)), reveal, wide);
    if (m.openSuspect >= 0) {
      m.page = m.openSuspect / g.suspectCapacity;
      m.openSuspect = -1;
    }
  }
  const int capacity = g.suspectCapacity, pages = (c->count + capacity - 1) / capacity;
  m.page = std::clamp(m.page, 0, pages - 1);
  if (m.view == View::Verdict) {
    const bool solved = carnivoro::caseStatus(state, state.caseId) == carnivoro::Status::Solved;
    const int remaining = carnivoro::remaining(state, state.caseId);
    const int leftW = wide ? content.width / 2 - 12 : content.width;
    const auto hero =
        fui::makeRect(content.x, content.y, leftW, wide ? content.height : content.height - (closed ? 182 : 124));
    const int mark = wide ? 48 : solved ? 96 : 128;
    if (solved) {
      const auto check = fui::makeRect(wide ? hero.x + 4 : hero.x + (hero.width - mark) / 2, hero.y + 4, mark, mark);
      t.stroke(check, black, 5);
      icon(t, check.inset({12, 12, 12, 12}), extra_check_32);
    } else if (closed)
      art(t, fui::makeRect(wide ? hero.x + 4 : hero.x + (hero.width - mark) / 2, hero.y + 4, mark, mark),
          mysteryart::BrokenHeart);
    else {
      const auto x = fui::makeRect(wide ? hero.x + 4 : hero.x + (hero.width - mark) / 2, hero.y + 4, mark, mark);
      t.stroke(x, black, 5);
      const int pad = mark / 4;
      t.line({int16_t(x.x + pad), int16_t(x.y + pad)}, {int16_t(x.right() - pad), int16_t(x.bottom() - pad)}, 12,
             black);
      t.line({int16_t(x.right() - pad), int16_t(x.y + pad)}, {int16_t(x.x + pad), int16_t(x.bottom() - pad)}, 12,
             black);
    }
    pixelui::text(t,
                  wide ? fui::makeRect(hero.x + 64, hero.y, hero.width - 64, 48)
                       : fui::makeRect(hero.x, hero.y + mark + 16, hero.width, 48),
                  solved   ? tr(STR_CAR_SOLVED)
                  : closed ? tr(STR_CAR_RESULT_FAILED)
                           : tr(STR_CAR_RESULT_WRONG),
                  wide ? 1 : 2, fui::Color::Black, wide ? fui::TextAlign::Left : fui::TextAlign::Center);
    text(t, fui::makeRect(hero.x + 8, wide ? hero.y + 58 : hero.y + mark + 68, hero.width - 16, 42),
         solved   ? tr(STR_CAR_RESULT_SOLVED_COPY)
         : closed ? tr(STR_CAR_RESULT_FAILED_COPY)
                  : tr(STR_CAR_RESULT_WRONG_COPY),
         4, 2, fui::TextAlign::Center);
    const bool lifeBeforeCast = !closed && !wide;
    const int lifeY = lifeBeforeCast ? hero.y + mark + 118 : hero.bottom() - 48;
    const int top = wide ? hero.y + 110 : hero.y + mark + 118 + (lifeBeforeCast ? 54 : 0);
    const auto panel = fui::makeRect(hero.x, top, hero.width, hero.bottom() - top - (lifeBeforeCast ? 0 : 58));
    t.stroke(panel, black, 2);
    if (solved) {
      int guilty = 0;
      while (guilty < c->count && !((m.solution.guilty >> guilty) & 1)) ++guilty;
      caps(t, fui::makeRect(panel.x + 8, panel.y + 8, panel.width - 16, 34),
           c->culprits == 1 ? animalName(guilty) : tr(STR_CAR_CULPRIT_BADGE), 1, fui::TextAlign::Center);
      int shown = 0;
      for (int n = 0; n < c->count; ++n)
        if ((m.solution.guilty >> n) & 1) {
          const int w = panel.width / c->culprits;
          portrait(t, fui::makeRect(panel.x + shown * w + 8, panel.y + 40, w - 16, panel.height - 74), n);
          ++shown;
        }
      const auto badge = fui::makeRect(panel.x + 10, panel.bottom() - 36, panel.width - 20, 30);
      t.fill(badge, black);
      text(t, badge, tr(STR_CAR_CULPRIT_BADGE), 1, 1, fui::TextAlign::Center, fui::Color::White);
    } else {
      text(t, fui::makeRect(panel.x + 8, panel.y + 8, panel.width - 16, 42),
           closed ? tr(STR_CAR_RESULT_AVAILABLE) : tr(STR_CAR_RESULT_REVIEW), 4, 2, fui::TextAlign::Center);
      portraitRow(t, fui::makeRect(panel.x + 8, panel.y + 54, panel.width - 16, panel.height - 62), m.page * capacity,
                  std::min(capacity, int(c->count) - m.page * capacity));
    }
    const auto lives = fui::makeRect(hero.x, lifeY, hero.width, 42);
    t.stroke(lives, black, 2);
    icon(t, fui::makeRect(lives.x + 16, lives.y + 6, 30, 30), icon_hearts_32);
    std::snprintf(m.formatted, sizeof(m.formatted), remaining == 1 ? tr(STR_CAR_RESULT_LIFE) : tr(STR_CAR_RESULT_LIVES),
                  remaining);
    text(t, fui::makeRect(lives.x + 60, lives.y, lives.width - 72, lives.height), m.formatted, 4, 1,
         fui::TextAlign::Center);
    const auto controls =
        wide ? fui::makeRect(hero.right() + 24, content.y + 48, content.width - hero.width - 24, content.height - 48)
             : fui::makeRect(content.x, content.bottom() - (closed ? 170 : 112), content.width, closed ? 170 : 112);
    const int h = 48;
    button(t, g, fui::makeRect(controls.x, controls.y, controls.width, h),
           !closed  ? tr(STR_CAR_RETRY)
           : solved ? tr(STR_CAR_NEXT)
                    : tr(STR_CAR_VIEW_SOLUTION),
           !closed || solved ? Continue : ViewSolution, 0, true);
    button(t, g, fui::makeRect(controls.x, controls.y + 58, controls.width, h),
           !closed  ? tr(STR_UI_BACK_TO_MENU_398BBB)
           : solved ? tr(STR_CAR_VIEW_SOLUTION)
                    : tr(STR_CAR_REPLAY),
           !closed  ? Menu
           : solved ? ViewSolution
                    : Replay);
    if (closed)
      button(t, g, fui::makeRect(controls.x, controls.y + 116, controls.width, h),
             solved ? tr(STR_CAR_REPLAY) : tr(STR_CAR_OTHER_CASE), solved ? Replay : Campaign);
    return;
  }
  if (m.view == View::Intro) {
    t.stroke(content, black, 2);
    text(t, fui::makeRect(content.x + 8, content.y + 8, (wide ? content.width / 2 : content.width) - 16, 48),
         tr(STR_CAR_THE_CASE), 2, 1, fui::TextAlign::Center);
    const int first = 0, count = std::min(3, int(c->count));
    const char* labels[] = {tr(STR_CAR_FACT_SUSPECTS), tr(STR_CAR_FACT_CULPRITS), tr(STR_CAR_FACT_LIES)};
    const int values[] = {c->count, c->culprits, c->lies};
    const freeink::Icon* marks[] = {&extra_information_32, &extra_notes_32, &icon_hearts_32};
    int factsX = content.x + 12, factsY = content.y + 222, factsW = content.width - 24;
    if (wide) {
      const int leftW = content.width / 2;
      portraitRow(t, fui::makeRect(content.x + 8, content.y + 56, leftW - 16, content.height - 170), first, count);
      factsX = content.x + leftW + 8;
      factsY = content.y + 58;
      factsW = content.width - leftW - 20;
    } else {
      portraitRow(t, fui::makeRect(content.x + 8, content.y + 62, content.width - 16, 150), first, count);
    }
    for (int n = 0; n < 3; ++n) {
      const auto box = fui::makeRect(factsX, factsY + n * 48, factsW, 44);
      t.stroke(box, black, 2);
      icon(t, fui::makeRect(box.x + 8, box.y + 6, 30, 30), *marks[n]);
      if (n == 0)
        std::snprintf(m.formatted, sizeof(m.formatted), labels[n], values[n]);
      else
        std::snprintf(m.formatted, sizeof(m.formatted), labels[n], values[n],
                      n == 1 ? (values[n] == 1 ? tr(STR_CAR_CULPRIT_ONE) : tr(STR_CAR_CULPRIT_MANY))
                             : (values[n] == 1 ? tr(STR_CAR_LIE_ONE) : tr(STR_CAR_LIE_MANY)));
      text(t, fui::makeRect(box.x + 52, box.y, box.width - 60, 44), m.formatted, 4, 2);
    }
    if (!wide) {
      const auto objective =
          fui::makeRect(content.x + 8, factsY + 150, content.width - 16, content.bottom() - 72 - factsY - 150);
      t.stroke(objective, black, 2);
      caps(t, fui::makeRect(objective.x + 12, objective.y + 6, objective.width - 24, 30), tr(STR_CAR_OBJECTIVE), 1);
      text(t,
           fui::makeRect(objective.x + 12, objective.y + 44, objective.width - 24,
                         std::max(30, int(objective.height) - 100)),
           tr(STR_CAR_OBJECTIVE_COPY), 4, 3);
      text(t, fui::makeRect(objective.x + 12, objective.bottom() - 64, objective.width - 24, 56),
           tr(STR_CAR_INDEPENDENT), 4, 3);
    }
    button(t, g, fui::makeRect(content.x, content.bottom() - 64, content.width, 64), tr(STR_CAR_INVESTIGATE), Start, 0,
           true, false, &extra_carnivoro_search_32);
    return;
  }
  if (m.view == View::Accuse) {
    const int controlsW = wide ? 228 : content.width;
    const auto cards = wide ? fui::makeRect(content.x, content.y, content.width - controlsW - 16, content.height)
                            : fui::makeRect(content.x, content.y, content.width, content.height - 208);
    const auto controls = wide ? fui::makeRect(cards.right() + 16, content.y, controlsW, content.height)
                               : fui::makeRect(content.x, cards.bottom() + 4, content.width, 204);
    text(t, fui::makeRect(cards.x, cards.y, cards.width, 44), tr(STR_CAR_ACCUSE_TITLE), 1);
    std::snprintf(m.formatted, sizeof(m.formatted), tr(STR_CAR_PICK_HELP), c->culprits);
    text(t, fui::makeRect(cards.x, cards.y + 46, cards.width, 30), m.formatted, 4);
    const int rowH = (cards.height - 84) / capacity;
    for (int n = 0; n < capacity; ++n) {
      const int id = m.page * capacity + n;
      if (id >= c->count) break;
      const auto box = fui::makeRect(cards.x, cards.y + 84 + n * rowH, cards.width, rowH - 8);
      const bool selected = (state.picks >> id) & 1;
      t.fill(box, selected ? black : white);
      t.stroke(box, black, 2);
      const int side = std::min(88, box.height - 8);
      portrait(t, fui::makeRect(box.x + 8, box.y + 4, side, side), id, selected);
      text(t, fui::makeRect(box.x + side + 24, box.y + 6, box.width - side - 88, box.height - 12), animalName(id), 1, 1,
           fui::TextAlign::Left, selected ? fui::Color::White : fui::Color::Black);
      checkbox(t, fui::makeRect(box.right() - 48, box.y + (box.height - 32) / 2, 32, 32), selected, selected);
      hit(g, box, Pick, id);
    }
    text(t, fui::makeRect(controls.x, controls.y, controls.width, wide ? 56 : 28), tr(STR_CAR_CONFIRM_HELP), 4,
         wide ? 3 : 1);
    std::snprintf(m.formatted, sizeof(m.formatted), tr(STR_CAR_SELECTED_COUNT), carnivoro::population(state.picks),
                  c->culprits);
    text(t, fui::makeRect(controls.x, controls.y + (wide ? 60 : 32), controls.width, 28), m.formatted, 0, 1,
         fui::TextAlign::Center);
    pager(t, g, fui::makeRect(controls.x, controls.bottom() - 140, controls.width, 44), m.page, pages);
    const auto confirm = fui::makeRect(controls.x, controls.bottom() - 86, controls.width, 40);
    if (!closed && carnivoro::population(state.picks) == c->culprits)
      button(t, g, confirm, tr(STR_CAR_CONFIRM), Confirm, 0, true, false, &extra_carnivoro_gavel_32);
    else {
      t.stroke(confirm, black, 1);
      text(t, confirm, tr(STR_CAR_CONFIRM), 0, 1, fui::TextAlign::Center);
    }
    button(t, g, fui::makeRect(controls.x, controls.bottom() - 38, controls.width, 38), tr(STR_CAR_BACK_CASE), Back);
    return;
  }
  if (m.view == View::Statements || m.view == View::Solution) {
    const int footer = reveal ? (explainSolution ? 224 : 160) : 124;
    const int cardWidth = content.width - (wide ? 244 : 0);
    const auto controls = wide ? fui::makeRect(content.x + cardWidth + 16, content.y, 228, content.height)
                               : fui::makeRect(content.x, content.bottom() - footer, content.width, footer);
    int required = 8 * (capacity - 1);
    for (int n = 0; n < capacity; ++n) {
      const int id = m.page * capacity + n;
      required += id < c->count ? statementHeight(t, *c, m, cardWidth, id, reveal, wide) : (reveal ? 138 : 146);
    }
    const int padding =
        wide ? 0 : std::max(0, (controls.y - content.y - (explainSolution ? 10 : 20) - required) / capacity);
    int y = content.y;
    for (int n = 0; n < capacity; ++n) {
      const int id = m.page * capacity + n;
      if (id >= c->count) break;
      const int height = statementHeight(t, *c, m, cardWidth, id, reveal, wide) + padding;
      statementCard(t, g, m, *c, fui::makeRect(content.x, y, cardWidth, height), id, reveal, wide);
      y += height + 8;
    }
    if (explainSolution) {
      int culprit = 0;
      while (culprit < c->count && !((m.solution.guilty >> culprit) & 1)) ++culprit;
      std::snprintf(m.formatted, sizeof(m.formatted), tr(STR_CAR_SOLUTION_THREE), animalName(culprit));
      text(t, fui::makeRect(controls.x, controls.y, controls.width, wide ? 96 : 64), m.formatted, 4, 3);
    }
    pager(t, g,
          fui::makeRect(controls.x, controls.y + (explainSolution ? (wide ? 104 : 64) : (wide ? 0 : -8)),
                        controls.width, 44),
          m.page, pages);
    const int gap = 8, buttonW = wide ? controls.width : (controls.width - gap) / 2;
    const int top = wide ? controls.bottom() - (reveal ? 164 : 112) : controls.y + (explainSolution ? 116 : 52);
    button(t, g, fui::makeRect(controls.x, top, buttonW, 48), tr(STR_CAR_OVERVIEW), ShowOverview);
    button(t, g, fui::makeRect(wide ? controls.x : controls.x + buttonW + gap, wide ? top + 56 : top, buttonW, 48),
           reveal ? tr(STR_CAR_NEXT) : tr(STR_CAR_ACCUSE), reveal ? Continue : Accuse, 0, true);
    if (reveal)
      button(t, g, fui::makeRect(controls.x, controls.bottom() - 48, controls.width, 48), tr(STR_CAR_REPLAY), Replay, 0,
             false, false, &extra_rotate_cw_32);
  }
}
}  // namespace carnivoroui
