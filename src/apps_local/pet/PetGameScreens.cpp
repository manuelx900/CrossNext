#include <cstdlib>

#include "PetPolishIcons.h"
#include "PetWidgets.h"

namespace petui {
using namespace widget;
namespace {
const char* gameName(pet::Game game) {
  static constexpr StrId names[] = {StrId::STR_PET_PAIRS, StrId::STR_PET_SEQUENCE, StrId::STR_PET_PUZZLE};
  return I18N.get(names[unsigned(game)]);
}
void dashedFrame(toybox::Screen& s, fui::Rect r, fui::Paint color, int thickness = 2) {
  const int dash = std::max(3, int(r.width) / 10), step = dash * 2;
  for (int x = r.x; x < r.right(); x += step) {
    const int length = std::min(dash, int(r.right()) - x);
    s.target().fill(rect(x, r.y, length, thickness), color);
    s.target().fill(rect(x, r.bottom() - thickness, length, thickness), color);
  }
  for (int y = r.y; y < r.bottom(); y += step) {
    const int length = std::min(dash, int(r.bottom()) - y);
    s.target().fill(rect(r.x, y, thickness, length), color);
    s.target().fill(rect(r.right() - thickness, y, thickness, length), color);
  }
}
void puzzleMotif(toybox::Screen& s, fui::Rect r, bool inverse = false) {
  const auto color = inverse ? paper : ink;
  const int side = (r.width - 8) / 3;
  for (int i = 0; i < 8; ++i)
    s.target().stroke(rect(r.x + i % 3 * (side + 4), r.y + i / 3 * (side + 4), side, side), color, 2);
  dashedFrame(s, rect(r.x + 2 * (side + 4), r.y + 2 * (side + 4), side, side), color);
}
const char* sequenceHint(pet::GamePhase phase) {
  switch (phase) {
    case pet::GamePhase::Playing:
      return tr(STR_PET_REPEAT);
    case pet::GamePhase::Memorize:
      return tr(STR_PET_REMEMBER);
    case pet::GamePhase::RoundComplete:
      return tr(STR_PET_GAME_ROUND_WIN);
    case pet::GamePhase::Finished:
      return tr(STR_PET_GAME_COMPLETE);
  }
  return "";
}
void symbol(toybox::Screen& s, fui::Rect r, unsigned value) {
  static constexpr const freeink::Icon* icons[] = {&icon_cat_nature_32, &extra_moon_64, &extra_reading_flame_64};
  icon(s, r, *icons[value % 3]);
}
void card(toybox::Screen& s, fui::Rect r, bool focus, bool hidden) {
  s.target().fill(r, paper);
  s.target().fill(rect(r.x + 4, r.y + 5, r.width - 4, r.height - 5), ink, 7);
  const auto front = rect(r.x, r.y, r.width - 5, r.height - 6);
  s.target().fill(front, paper, 7);
  s.target().stroke(front, ink, focus ? 4 : 3, 7);
  s.target().stroke(front.inset({8, 8, 8, 8}), ink, 1, 3);
  if (!hidden) return;
  const int length = std::min(9, int(r.width) / 9);
  for (int right : {0, 1})
    for (int bottom : {0, 1}) {
      const int x = right ? r.right() - 19 : r.x + 16, y = bottom ? r.bottom() - 22 : r.y + 16;
      s.target().fill(rect(right ? x - length : x, y, length + 3, 3), ink);
      s.target().fill(rect(x, bottom ? y - length : y, 3, length + 3), ink);
    }
}
void cardMotif(toybox::Screen& s, fui::Rect r, pet::Game kind, bool inverse = false) {
  if (kind == pet::Game::Puzzle) {
    puzzleMotif(s, r, inverse);
    return;
  }
  const auto foreground = inverse ? paper : ink, background = inverse ? ink : paper;
  if (kind == pet::Game::Sequence) {
    const int side = (r.height - 8) / 3;
    for (int i = 0; i < 3; ++i) {
      const auto number = rect(r.x, r.y + i * (side + 4), side, side);
      s.target().stroke(number, foreground, 2);
      static constexpr const char* digits[] = {"1", "2", "3"};
      text(s, number, digits[i], 3, inverse, fui::TextAlign::Center);
      s.target().fill(rect(number.right() + 7, number.y + side / 3, r.width - side - 7, 3), foreground);
      s.target().fill(rect(number.right() + 7, number.y + side * 2 / 3, r.width - side - 15, 3), foreground);
    }
    return;
  }
  const auto scaled = [&](int x, int y, int width, int height) {
    return rect(r.x + x * r.width / 80, r.y + y * r.height / 80,
                std::max(1, (x + width) * r.width / 80 - x * r.width / 80),
                std::max(1, (y + height) * r.height / 80 - y * r.height / 80));
  };
  s.target().stroke(scaled(24, 18, 49, 59), foreground, 3, 3);
  for (int y = 7; y <= 72; ++y) {
    const int left = y < 15 ? 53 - (y - 7) * 45 / 8 : 8 + (y - 15) * 10 / 57;
    const int right = y < 63 ? 53 + (y - 7) * 12 / 56 : 65 - (y - 63) * 47 / 9;
    s.target().fill(scaled(left, y, right - left + 1, 1), foreground);
    if (y >= 12 && y <= 67 && right - left > 8) s.target().fill(scaled(left + 4, y, right - left - 7, 1), background);
  }
  static constexpr const char* heart[] = {" ## ## ", "#######", "#######", " ##### ", "  ###  ", "   #   "};
  static constexpr const char* star[] = {"   #   ", "   #   ", "#######", " ##### ", " ##### ", " ## ## ", " #   # "};
  {
    const int rows = kind == pet::Game::Pairs ? 6 : 7;
    for (int y = 0; y < rows; ++y)
      for (int x = 0; x < 7; ++x)
        if ((kind == pet::Game::Pairs ? heart[y] : star[y])[x] == '#')
          s.target().fill(scaled(25 + x * 4, 29 + y * 4, 4, 4), foreground);
  }
}
void errorMark(toybox::Screen& s, fui::Rect r) {
  s.target().stroke(r, ink, 3, uint8_t(r.width / 2));
  const int inset = r.width / 3;
  s.target().line({int16_t(r.x + inset), int16_t(r.y + inset)},
                  {int16_t(r.right() - inset), int16_t(r.bottom() - inset)}, 3, ink);
  s.target().line({int16_t(r.right() - inset), int16_t(r.y + inset)},
                  {int16_t(r.x + inset), int16_t(r.bottom() - inset)}, 3, ink);
}
void statistic(toybox::Screen& s, fui::Rect r, const char* label, const char* value, const freeink::Icon& mark,
               bool compact, bool errors = false, bool bars = false) {
  const int side = compact ? 28 : 42, left = compact ? 8 : 14;
  const auto image = rect(r.x + left, r.y + (r.height - side) / 2, side, side);
  if (errors)
    errorMark(s, image);
  else if (bars) {
    const int bar = image.width / 4;
    for (int i = 0; i < 3; ++i)
      s.target().fill(
          rect(image.x + i * (bar + 2), image.bottom() - image.height * (i + 1) / 3, bar, image.height * (i + 1) / 3),
          ink);
  } else
    icon(s, image, mark);
  const int x = image.right() + (compact ? 8 : 14);
  if (compact) {
    text(s, rect(x, r.y + 5, r.width - (x - r.x) - 70, r.height - 10), label, 3);
    text(s, rect(r.right() - 65, r.y + 4, 57, r.height - 8), value, 0, false, fui::TextAlign::Right);
  } else {
    text(s, rect(x, r.y + 10, r.right() - x - 8, 24), label, 0);
    text(s, rect(x, r.y + 35, r.right() - x - 8, r.height - 41), value, 1);
  }
}
void stats(toybox::Screen& s, const Model& m, Work& w, fui::Rect r, bool compact) {
  const auto& g = *m.game;
  frame(s, r);
  const bool pairs = g.kind == pet::Game::Pairs;
  const int half = compact ? r.height / 2 : r.width / 2;
  const auto first = compact ? rect(r.x, r.y, r.width, half) : rect(r.x, r.y, half, r.height);
  const auto second =
      compact ? rect(r.x, r.y + half, r.width, r.height - half) : rect(r.x + half, r.y, r.width - half, r.height);
  unsigned found = 0;
  for (unsigned bit = 0; bit < 6; ++bit) found += (g.matched >> bit) & 1;
  const unsigned round = g.phase == pet::GamePhase::RoundComplete ? g.rounds : std::min(3u, unsigned(g.rounds) + 1);
  unsigned placed = 0;
  for (unsigned i = 0; i < 8; ++i) placed += g.puzzle[i] == i + 1;
  const bool puzzle = g.kind == pet::Game::Puzzle;
  std::snprintf(w.label, sizeof(w.label), "%u / %u", puzzle ? placed : pairs ? found / 2 : round, puzzle ? 8 : 3);
  statistic(s, first,
            puzzle  ? tr(STR_PET_PUZZLE_PLACED)
            : pairs ? tr(STR_PET_PAIRS)
                    : tr(STR_PET_GAME_ROUND),
            w.label, pairs ? extra_cards_64 : icon_mineflag_32, compact);
  unsigned value = g.mistakes;
  const char* label = tr(STR_PET_GAME_ERRORS);
  if (g.kind == pet::Game::Sequence) {
    value = g.sequenceLength() - (g.phase == pet::GamePhase::RoundComplete ? 1 : 0);
    label = tr(STR_PET_GAME_LENGTH);
  } else if (g.kind == pet::Game::Puzzle) {
    value = g.moves;
    label = tr(STR_PET_GAME_MOVES);
  }
  std::snprintf(w.label, sizeof(w.label), "%u", value);
  statistic(s, second, label, w.label, g.kind == pet::Game::Sequence ? extra_power_64 : extra_cards_64, compact, pairs,
            g.kind == pet::Game::Sequence);
  if (compact)
    s.target().line({int16_t(r.x + 8), int16_t(r.y + half)}, {int16_t(r.right() - 8), int16_t(r.y + half)}, 1, ink);
  else
    s.target().line({int16_t(r.x + half), int16_t(r.y + 14)}, {int16_t(r.x + half), int16_t(r.bottom() - 14)}, 2, ink);
}
void hint(toybox::Screen& s, fui::Rect r, const char* copy) {
  frame(s, r);
  icon(s, rect(r.x + 12, r.y + (r.height - 30) / 2, 30, 30), extra_light_on_64);
  s.target().line({int16_t(r.x + 54), int16_t(r.y + 9)}, {int16_t(r.x + 54), int16_t(r.bottom() - 9)}, 2, ink);
  prose(s, rect(r.x + 68, r.y + 7, r.width - 78, r.height - 14), copy, 2, fui::TextAlign::Center);
}
void giftMotif(toybox::Screen& s, fui::Rect r) {
  s.target().stroke(rect(r.x + 3, r.y + r.height / 3, r.width - 6, r.height * 2 / 3 - 2), ink, 3);
  s.target().stroke(rect(r.x, r.y + r.height / 3, r.width, r.height / 5), ink, 3);
  s.target().fill(rect(r.x + r.width / 2 - 2, r.y + r.height / 3, 4, r.height * 2 / 3), ink);
  s.target().stroke(rect(r.x + r.width / 7, r.y + 2, r.width / 3, r.height / 3), ink, 3, 5);
  s.target().stroke(rect(r.x + r.width / 2, r.y + 2, r.width / 3, r.height / 3), ink, 3, 5);
}
void energyMotif(toybox::Screen& s, fui::Rect r, bool inverse) {
  static constexpr const char* pixels[] = {"      ##", "     ###", "    ### ", "   ###  ", "  ######", " #######",
                                           "######  ", "   ###  ", "  ###   ", " ###    ", " ##     ", "#       "};
  for (int y = 0; y < 12; ++y)
    for (int x = 0; x < 8; ++x)
      if (pixels[y][x] == '#')
        s.target().fill(rect(r.x + x * r.width / 8, r.y + y * r.height / 12, (x + 1) * r.width / 8 - x * r.width / 8,
                             (y + 1) * r.height / 12 - y * r.height / 12),
                        inverse ? paper : ink);
}
void pairBoard(toybox::Screen& s, const Model& m, Work& w, fui::Rect area) {
  const auto& g = *m.game;
  const int cols = 3, rows = 2;
  const int gap = 14,
            side = std::min((int(area.width) - gap * (cols - 1)) / cols, (int(area.height) - gap * (rows - 1)) / rows);
  const int x = area.x + (area.width - side * cols - gap * (cols - 1)) / 2;
  for (int i = 0; i < 6; ++i) {
    const auto r = rect(x + (i % cols) * (side + gap), area.y + (i / cols) * (side + gap), side, side);
    const bool matched = g.matched & (1u << i), visible = matched || g.picked == i || g.second == i;
    const bool focus = button(s, w, r, "", GamePick, i, false, nullptr, !g.finished() && !matched && g.second == 255);
    card(s, r, focus, !visible);
    const int size = std::min(66, side - 46);
    const auto center = rect(r.x + (r.width - size) / 2 - 2, r.y + (r.height - size) / 2 - 2, size, size);
    if (visible)
      symbol(s, center, g.cards[i]);
    else
      text(s, center, "?", 2, false, fui::TextAlign::Center);
    if (matched) icon(s, rect(r.right() - 31, r.bottom() - 34, 16, 16), extra_check_32);
  }
}
void sequenceBoard(toybox::Screen& s, const Model& m, Work& w, fui::Rect area) {
  const auto& g = *m.game;
  const bool complete = g.phase == pet::GamePhase::RoundComplete || g.finished();
  const unsigned count = g.sequenceLength() - (g.phase == pet::GamePhase::RoundComplete ? 1 : 0);
  const int height = std::min(106, (int(area.width) - 6 * (int(count) - 1)) / int(count) * 6 / 5);
  const auto series = rect(area.x, area.y + 5, area.width, height);
  for (unsigned i = 0; i < count; ++i) {
    const auto r = cell(series, int(i), int(count), 6);
    const bool known = g.showing() || complete || i < g.step;
    card(s, r, false, !known);
    if (known)
      symbol(s, r.inset({15, 15, 20, 18}), g.sequence[i]);
    else
      text(s, r.inset({12, 10, 16, 10}), "?", 1, false, fui::TextAlign::Center);
  }
  if (g.showing() || complete) {
    const auto note = rect(area.x + area.width / 10, series.bottom() + 24, area.width * 8 / 10, 56);
    hint(s, note,
         g.finished() ? tr(STR_PET_GAME_COMPLETE)
         : complete   ? tr(STR_PET_GAME_ROUND_WIN)
                      : tr(STR_PET_GAME_OBSERVE));
    return;
  }
  const int top = series.bottom() + 24;
  const auto choices = rect(area.x, top, area.width, std::min(108, int(area.bottom()) - top));
  for (int i = 0; i < 3; ++i) {
    const auto r = cell(choices, i, 3, 14);
    const bool focus = button(s, w, r, "", GamePick, i);
    card(s, r, focus, false);
    symbol(s, r.inset({20, 20, 24, 24}), unsigned(i));
  }
}
void puzzleBoard(toybox::Screen& s, const Model& m, Work& w, fui::Rect area, bool wide) {
  const auto& g = *m.game;
  w.puzzleLayout = petinput::puzzleLayout(area, wide);
  const auto& layout = w.puzzleLayout;
  s.target().stroke(layout.frame, ink, 4, 6);
  unsigned blank = 0;
  while (blank < 9 && g.puzzle[blank]) ++blank;
  for (int i = 0; i < 9; ++i) {
    const auto r = layout.tile(unsigned(i));
    if (!g.puzzle[i]) {
      dashedFrame(s, r.inset({2, 2, 2, 2}), ink, 2);
      continue;
    }
    const bool adjacent = blank < 9 && std::abs(i / 3 - int(blank / 3)) + std::abs(i % 3 - int(blank % 3)) == 1;
    const bool focus = button(s, w, r, "", GamePick, i, false, nullptr, g.phase == pet::GamePhase::Playing && adjacent);
    s.target().fill(r, paper);
    s.target().fill(rect(r.x + 3, r.y + 3, r.width - 3, r.height - 3), ink, 4);
    const auto face = rect(r.x, r.y, r.width - 3, r.height - 3);
    s.target().fill(face, paper, 4);
    s.target().stroke(face, ink, focus ? 4 : 3, 4);
    std::snprintf(w.label, sizeof(w.label), "%u", unsigned(g.puzzle[i]));
    text(s, face.inset({4, 4, 4, 4}), w.label, 2, false, fui::TextAlign::Center);
  }
  if (!wide) {
    const auto note = rect(area.x, area.bottom() - 64, area.width, 64);
    hint(s, note, g.finished() ? tr(STR_PET_GAME_COMPLETE) : tr(STR_PET_PUZZLE_HELP));
  }
}
}  // namespace

void games(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const bool wide = r.width > r.height;
  const auto blocked = pet::playBlock(*m.profile);
  const auto offer = gameOffer(m);
  const bool reward = offer == GameOffer::Reward;
  const char* offerLabel = nullptr;
  switch (offer) {
    case GameOffer::Blocked:
      offerLabel = tr(STR_PET_GAME_OFFER_BLOCKED);
      break;
    case GameOffer::Reward:
      offerLabel = tr(STR_PET_GAME_OFFER_REWARD);
      break;
    case GameOffer::Practice:
      offerLabel = tr(STR_PET_GAME_PRACTICE);
      break;
  }
  const auto status = rect(r.x, r.y, r.width, wide ? 44 : 84);
  if (wide) {
    for (int i = 0; i < 2; ++i) {
      const auto box = cell(status, i, 2);
      frame(s, box);
      icon(s, rect(box.x + 10, box.y + 5, 24, 24), i ? extra_power_64 : icon_hearts_32);
      text(s, rect(box.x + 44, box.y + 3, box.width - 145, 25), i ? tr(STR_PET_ENERGY) : tr(STR_PET_JOY), 3);
      const unsigned value = i ? m.profile->energy : m.profile->joy;
      std::snprintf(w.label, sizeof(w.label), "%u / 100", value);
      text(s, rect(box.right() - 95, box.y + 3, 85, 25), w.label, 3, false, fui::TextAlign::Right);
      dashboard::bar(s, rect(box.x + 10, box.bottom() - 10, box.width - 20, 7), value, 100);
    }
  } else {
    metric(s, w, cell(status, 0, 2), tr(STR_PET_JOY), m.profile->joy, icon_hearts_32);
    metric(s, w, cell(status, 1, 2), tr(STR_PET_ENERGY), m.profile->energy, extra_power_64);
  }
  const auto offerBox = rect(r.x, status.bottom() + GAP, r.width, wide ? 64 : 80);
  frame(s, offerBox);
  const int markSide = wide ? 30 : 38, copyX = offerBox.x + (wide ? 64 : 76);
  giftMotif(s, rect(offerBox.x + 13, offerBox.y + (offerBox.height - markSide) / 2, markSide, markSide));
  s.target().line({int16_t(copyX - 12), int16_t(offerBox.y + 10)},
                  {int16_t(copyX - 12), int16_t(offerBox.bottom() - 10)}, 1, ink);
  text(s, rect(copyX, offerBox.y + 6, (offerBox.right() - copyX) * 42 / 100, 23), offerLabel, 3);
  if (m.clockKnown && m.today > 0 && m.today >= m.profile->gameDay && m.writable && !m.historyChanged)
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GAME_PRIZES_USED),
                  m.today == m.profile->gameDay ? std::min(3u, unsigned(m.profile->gamesToday)) : 0u);
  else
    std::snprintf(w.label, sizeof(w.label), "%s", tr(STR_PET_GAME_PRIZES_UNKNOWN));
  text(s,
       rect(copyX + (offerBox.right() - copyX) * 42 / 100, offerBox.y + 6, (offerBox.right() - copyX) * 58 / 100 - 8,
            23),
       w.label, 3, false, fui::TextAlign::Right);
  prose(s, rect(copyX, offerBox.y + 31, offerBox.right() - copyX - 8, offerBox.height - 35),
        blocked != pet::PlayBlock::None ? playBlockText(blocked)
        : m.profile->joy < 40           ? tr(STR_PET_LOW_MOOD_HELP)
                                        : tr(STR_PET_GAME_RULES_BRIEF),
        wide ? 1 : 2);
  const int headerH = wide ? 34 : 46, headerY = offerBox.bottom() + GAP;
  icon(s, rect(r.x + 3, headerY + 3, wide ? 26 : 36, wide ? 26 : 36), GAMEPAD_ICON);
  pixelui::copyCaps(w.label, sizeof(w.label), tr(STR_PET_MINIGAMES));
  const int headingFont = wide ? 0 : 2;
  const int titleW = s.target().measureText(headingFont, w.label, {}).width;
  const int titleX = r.x + (wide ? 38 : 48);
  text(s, rect(titleX, headerY, r.right() - titleX, headerH), w.label, headingFont);
  const int lineX = titleX + titleW + 12;
  if (lineX < r.right() - 8)
    s.target().line({int16_t(lineX), int16_t(headerY + headerH / 2)},
                    {int16_t(r.right() - 2), int16_t(headerY + headerH / 2)}, 2, ink);
  const auto footer = rect(r.x, r.bottom() - 54, r.width, 54);
  const auto cards = rect(r.x, headerY + headerH + 4, r.width, footer.y - GAP - headerY - headerH - 4);
  static constexpr StrId descriptions[] = {StrId::STR_PET_PAIRS_DESC, StrId::STR_PET_SEQUENCE_DESC,
                                           StrId::STR_PET_PUZZLE_DESC};
  static constexpr StrId difficulties[] = {StrId::STR_PET_GAME_EASY, StrId::STR_PET_GAME_MEDIUM,
                                           StrId::STR_PET_GAME_HARD};
  for (int i = 0; i < 3; ++i) {
    const auto box = cell(cards, i, 3, 7);
    const bool inverse = button(s, w, box, "", SelectGame, i, int(m.selectedGame) == i);
    const auto color = inverse ? paper : ink;
    pixelui::copyCaps(w.label, sizeof(w.label), gameName(pet::Game(i)));
    const int font = s.target().measureText(0, w.label, {}).width <= box.width - 8 ? 0 : 3;
    text(s, rect(box.x + 4, box.y + 7, box.width - 8, wide ? 22 : 29), w.label, font, inverse, fui::TextAlign::Center);
    if (wide) {
      const int lineH = (box.height - 36) / 3;
      const unsigned gain = std::min(pet::gameMoodGain(*m.profile, pet::Game(i)), 100u - m.profile->joy);
      std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GAME_GAIN_VALUE), gain);
      const char* lines[] = {I18N.get(difficulties[i]),
                             offer == GameOffer::Blocked ? offerLabel
                             : reward                    ? w.label
                                                         : tr(STR_PET_GAME_PRACTICE),
                             offer == GameOffer::Blocked ? ""
                             : reward                    ? tr(STR_PET_GAME_COST)
                                                         : tr(STR_PET_GAME_NO_COST)};
      for (int n = 0; n < 3; ++n)
        text(s, rect(box.x + 8, box.y + 31 + n * lineH, box.width - 16, lineH), lines[n], 3, inverse,
             fui::TextAlign::Center);
      continue;
    } else {
      const int side = std::min(64, int(box.width) - 24);
      cardMotif(s, rect(box.x + (box.width - side) / 2, box.y + 48, side, side), pet::Game(i), inverse);
      for (int x = box.x + 12; x < box.right() - 10; x += 5) s.target().fill(rect(x, box.y + 124, 2, 2), color);
      const auto badge = rect(box.x + 12, box.y + 136, box.width - 24, 26);
      s.target().stroke(badge, color, 2);
      text(s, badge.inset({1, 3, 1, 3}), I18N.get(difficulties[i]), 3, inverse, fui::TextAlign::Center);
      prose(s, rect(box.x + 10, box.y + 174, box.width - 20, box.height - 248), I18N.get(descriptions[i]), 3,
            fui::TextAlign::Center, inverse);
      s.target().line({int16_t(box.x + 11), int16_t(box.bottom() - 71)},
                      {int16_t(box.right() - 11), int16_t(box.bottom() - 71)}, 1, color);
    }
    const unsigned gain = std::min(pet::gameMoodGain(*m.profile, pet::Game(i)), 100u - m.profile->joy);
    std::snprintf(w.label, sizeof(w.label), tr(STR_PET_GAME_GAIN_VALUE), gain);
    const int lineH = wide ? 19 : 23, bottomInset = wide ? 5 : 10;
    const int rewardY = box.bottom() - lineH * 2 - bottomInset - 4;
    if (offer == GameOffer::Blocked) {
      text(s, rect(box.x + 4, rewardY, box.width - 8, lineH), offerLabel, 3, inverse, fui::TextAlign::Center);
    } else {
      icon(s, rect(box.x + 9, rewardY, 20, 20), icon_hearts_32, inverse);
      text(s, rect(box.x + 35, rewardY, box.width - 40, lineH), reward ? w.label : tr(STR_PET_GAME_PRACTICE), 3,
           inverse);
    }
    if (offer != GameOffer::Blocked) {
      energyMotif(s, rect(box.x + 11, rewardY + lineH + 4, 16, 20), inverse);
      text(s, rect(box.x + 35, rewardY + lineH + 4, box.width - 40, lineH),
           reward ? tr(STR_PET_GAME_COST) : tr(STR_PET_GAME_NO_COST), 3, inverse);
    }
  }
  const bool enabled = offer != GameOffer::Blocked;
  const bool inverse = button(s, w, footer, "", BeginGame, 0, true, nullptr, enabled);
  icon(s, rect(footer.x + 15, footer.y + 7, 40, 40), extra_dashboard_play_64, inverse);
  s.target().line({int16_t(footer.x + 70), int16_t(footer.y + 9)},
                  {int16_t(footer.x + 70), int16_t(footer.bottom() - 9)}, 1, inverse ? paper : ink);
  text(s, rect(footer.x + 82, footer.y + 4, footer.width - 94, footer.height - 8),
       enabled ? tr(STR_PET_START_GAME) : tr(STR_PET_GAME_CARE_FIRST), 0, inverse, fui::TextAlign::Center);
}
void play(toybox::Screen& s, const Model& m, Work& w, fui::Rect r) {
  const auto& g = *m.game;
  const bool wide = r.width > r.height;
  const int titleH = 108, statsH = 86;
  const int left = wide ? std::min(248, int(r.width) * 34 / 100) : r.width;
  const auto title = rect(r.x + 5, r.y + 5, left - (wide ? 10 : 102), 54);
  pixelui::copyCaps(w.label, sizeof(w.label), gameName(g.kind));
  const int titleFont = s.target().measureText(2, w.label, {}).width <= title.width   ? 2
                        : s.target().measureText(1, w.label, {}).width <= title.width ? 1
                                                                                      : 0;
  text(s, title, w.label, titleFont);
  if (!wide) {
    const auto mark = rect(r.right() - 88, r.y + 10, 78, 78);
    cardMotif(s, mark, g.kind);
  }
  const char* description = g.kind == pet::Game::Pairs    ? tr(STR_PET_PAIRS_DESC)
                            : g.kind == pet::Game::Puzzle ? tr(STR_PET_PUZZLE_DESC)
                                                          : sequenceHint(g.phase);
  prose(s, rect(r.x + 5, r.y + 62, left - 10, wide ? 40 : 36), description, 2);
  const auto counters = rect(r.x, r.y + titleH, left, wide ? 86 : statsH);
  stats(s, m, w, counters, wide);
  const auto footer = rect(r.x, r.bottom() - 54, r.width, 54);
  const bool collect = g.finished() && gameOffer(m) == GameOffer::Reward;
  const int receiptHint = collect ? 34 : 0;
  const auto board = wide ? rect(r.x + left + 18, r.y + 3, r.width - left - 18, r.height - 70)
                          : rect(r.x, counters.bottom() + 22, r.width, footer.y - counters.bottom() - 32 - receiptHint);
  switch (g.kind) {
    case pet::Game::Pairs:
      pairBoard(s, m, w, board);
      break;
    case pet::Game::Sequence:
      sequenceBoard(s, m, w, board);
      break;
    case pet::Game::Puzzle:
      puzzleBoard(s, m, w, board, wide);
      if (wide)
        prose(s,
              rect(r.x + 5, counters.bottom() + 10, left - 10, footer.y - counters.bottom() - 22 - (collect ? 50 : 0)),
              tr(STR_PET_PUZZLE_HELP), 4);
      break;
  }
  button(s, w, rect(footer.x, footer.y, 92, footer.height), tr(STR_PET_GAME_MENU), Back);
  if (collect)
    prose(s, rect(r.x, footer.y - (wide ? 50 : 30), wide ? left : r.width, wide ? 44 : 24), tr(STR_PET_GAME_CLAIM_HINT),
          wide ? 2 : 1);
  const auto primary = rect(footer.x + 100, footer.y, footer.width - 100, footer.height);
  switch (g.phase) {
    case pet::GamePhase::Finished:
      button(s, w, primary, collect ? tr(STR_PET_GAME_COLLECT) : tr(STR_PET_GAME_VIEW_RESULT), GameDone, 0, true);
      break;
    case pet::GamePhase::RoundComplete:
      button(s, w, primary, tr(STR_PET_GAME_NEXT_ROUND), GameNext, 0, true);
      break;
    case pet::GamePhase::Memorize:
      button(s, w, primary, tr(STR_PET_READY), GameNext, 0, true);
      break;
    case pet::GamePhase::Playing:
      if (g.kind == pet::Game::Puzzle)
        button(s, w, primary, tr(STR_PET_GAME_RESTART), GameReset, 0, true);
      else
        button(s, w, primary, tr(STR_PET_CONTINUE), GameNext, 0, true, nullptr,
               g.kind == pet::Game::Pairs && g.second != 255);
      break;
  }
}
}  // namespace petui
