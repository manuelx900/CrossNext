#include "WordleScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/GameGuide.h"
#include "../ui/GamePanel.h"
#include "../ui/PixelMenu.h"
namespace wordleui {
namespace {
constexpr auto ink = fui::Paint::solid(fui::Color::Black);
constexpr auto grey = fui::Paint::dither(fui::Color::LightGray);
void chrome(toybox::Screen& screen, const char* title, const char* right = nullptr) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.rightLabel = right;
  header.back = true;
  pixelui::header(screen, header);
  backui::publish(true, pixelui::geometry(screen.device()).back);
}
void text(fui::DrawTarget& target, fui::Rect box, const char* value, fui::FontId font = pixelui::kSmall, int lines = 1,
          fui::Color color = fui::Color::Black) {
  fui::TextStyle style;
  style.font = font;
  style.maxLines = lines;
  style.align = fui::TextAlign::Center;
  style.color = color;
  const auto* cut = toybox::cutForLineHeight(target.lineHeight(font));
  target.text(lines == 1 && cut ? toybox::inkCentred(box, *cut) : box, value, style);
}
void button(toybox::Screen& screen, fui::Rect box, const char* label, fui::ActionId action, int value = 0,
            bool selected = false) {
  if (selected)
    screen.target().fill(box, ink);
  else
    screen.target().stroke(box, ink, pixelui::kBorder);
  text(screen.target(), box.inset(fui::Insets{3, 6, 3, 6}), label, pixelui::kSmall, 1,
       selected ? fui::Color::White : fui::Color::Black);
  screen.frame().hit(box, action, value, fui::InputTouch);
}
void cell(fui::DrawTarget& target, fui::Rect box, char letter, Mark mark) {
  if (mark == Mark::Correct)
    target.fill(box, ink);
  else if (mark == Mark::Present)
    target.fill(box, grey);
  target.stroke(box, ink, 3);

  char label[3] = {letter, 0, 0};
  if (letter == '[') {
    label[0] = char(0xc3);
    label[1] = char(0x91);
  }
  if (letter)
    text(target, box, label, pixelui::kBody, 1, mark == Mark::Correct ? fui::Color::White : fui::Color::Black);
}
}  // namespace
KeyboardLayout buildGame(toybox::Screen& screen, const GameModel& model, Work& work) {
  const auto& game = *model.game;
  char result[24]{};
  if (game.over()) {
    if (game.status() == wordle::Game::Status::Won)
      std::snprintf(result, sizeof(result), "%d / 6", game.guesses());
    else
      std::snprintf(result, sizeof(result), "X / 6");
  }
  char answer[16]{};
  int at = 0;
  for (int n = 0; n < game.letters(); ++n) {
    if (game.answer()[n] == '[') {
      answer[at++] = char(0xc3);
      answer[at++] = char(0x91);
    } else
      answer[at++] = game.answer()[n];
  }
  chrome(screen, tr(STR_WORDLE_NAME));
  auto& target = screen.target();
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  const int gap = 6, letters = game.letters();
  const auto gridArea = wide ? fui::makeRect(body.x, body.y, body.width * 46 / 100, body.height)
                             : fui::makeRect(body.x, body.y, body.width, body.height - 246);
  const int tile =
      std::min({68, (int(gridArea.width) - (letters - 1) * gap) / letters, (int(gridArea.height) - 40 - 5 * gap) / 6});
  const int left = gridArea.x + (gridArea.width - letters * tile - (letters - 1) * gap) / 2, top = gridArea.y + 32;
  std::snprintf(work.label, sizeof(work.label), tr(STR_REDESIGN_ATTEMPT), std::min(6, game.guesses() + 1));
  pixelui::text(target, fui::makeRect(gridArea.x, gridArea.y, gridArea.width / 2, 28), model.date ? model.date : "", 0);
  pixelui::text(target, fui::makeRect(gridArea.x + gridArea.width / 2, gridArea.y, gridArea.width / 2, 28),
                game.over() ? result : work.label, 0, fui::Color::Black, fui::TextAlign::Right);

  for (int row = 0; row < 6; ++row)
    for (int col = 0; col < letters; ++col) {
      char letter = 0;
      Mark mark = Mark::Empty;
      if (row < game.guesses()) {
        letter = game.guess(row)[col];
        mark = game.marks(row)[col];
      } else if (row == game.guesses() && !game.over() && col < game.typed()) {
        letter = game.typing()[col];
        mark = Mark::Typed;
      }
      const auto box = fui::makeRect(left + col * (tile + gap), top + row * (tile + gap), tile, tile);
      cell(target, box, letter, mark);
      if (row == game.guesses() && !game.over()) target.stroke(box, ink, 4);
    }
  const int keyboardH = 3 * 64 + 2 * 4;
  const int groupH = keyboardH + 44;
  const int groupY = body.y + (body.height - groupH) / 2;
  const auto controls =
      wide && !game.over()
          ? fui::makeRect(gridArea.right() + 12, groupY + 44, body.right() - gridArea.right() - 12, keyboardH)
      : wide ? fui::makeRect(gridArea.right() + 12, body.y + 40, body.right() - gridArea.right() - 12, body.height - 40)
             : fui::makeRect(body.x, body.bottom() - 204, body.width, 204);
  if (!game.over()) {
    const auto legend = wide ? fui::makeRect(controls.x, groupY, controls.width, 32)
                             : fui::makeRect(body.x, controls.y - 36, body.width, 30);
    if (model.message) {
      text(target, legend, model.message, 3);
    } else {
      const StrId labels[] = {StrId::STR_REDESIGN_CORRECT, StrId::STR_REDESIGN_MISPLACED, StrId::STR_REDESIGN_ABSENT};
      for (int n = 0; n < 3; ++n) {
        const int width = legend.width / 3;
        const auto swatch = fui::makeRect(legend.x + n * width, legend.y + 5, 20, 20);
        if (n < 2) target.fill(swatch, n == 0 ? ink : grey);
        target.stroke(swatch, ink, 1);
        pixelui::text(target, fui::makeRect(swatch.right() + 6, legend.y, width - 28, 30), I18N.get(labels[n]), 3,
                      fui::Color::Black, fui::TextAlign::Center);
      }
    }
  }
  if (game.over()) {
    text(target, fui::makeRect(controls.x, controls.y, controls.width, 64),
         game.status() == wordle::Game::Status::Won ? tr(STR_WORDLE_WELL_DONE) : tr(STR_WORDLE_TRY_ANOTHER),
         pixelui::kBody);
    std::snprintf(work.label, sizeof(work.label), tr(STR_GUIDE_WORDLE_ANSWER), answer);
    text(target, fui::makeRect(controls.x, controls.y + 68, controls.width, 36), work.label, 0);
    const int width = (controls.width - 12) / 2;
    button(screen, fui::makeRect(controls.x, controls.bottom() - 64, width, 60), tr(STR_WORDLE_MENU), ActionArchive);
    button(screen, fui::makeRect(controls.x + width + 12, controls.bottom() - 64, width, 60), tr(STR_WORDLE_PLAY_AGAIN),
           model.canNext ? ActionNext : fui::NO_ACTION);
    return {};
  }
  auto k = keyboardLayout(controls, controls.y);

  wordlekeys::Tone tones[27];
  for (int n = 0; n < 27; ++n) {
    const auto mark = game.key(char('A' + n));
    tones[n] = mark == Mark::Correct   ? wordlekeys::Tone::Correct
               : mark == Mark::Present ? wordlekeys::Tone::Present
                                       : wordlekeys::Tone::Normal;
  }
  wordlekeys::draw(screen, controls, k, ActionKeyboard, tones);
  return k;
}
void buildMenu(toybox::Screen& screen, const MenuModel& model, Work& work) {
  chrome(screen, tr(STR_WORDLE_NAME));
  auto& target = screen.target();
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  const auto main = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 8, body.height) : body;
  const int tiles = model.letters;
  const int tile = std::min(58, (int(main.width) - 30) / tiles);
  const char* sample = tiles == 4   ? tr(STR_WORDLE_SAMPLE_4)
                       : tiles == 6 ? tr(STR_WORDLE_SAMPLE_6)
                                    : tr(STR_WORDLE_SAMPLE_5);
  const int x = main.x + (main.width - tiles * (tile + 4) + 4) / 2;
  for (int n = 0; n < tiles; ++n)
    cell(target, fui::makeRect(x + n * (tile + 4), main.y, tile, tile), sample[n],
         n == 0   ? Mark::Correct
         : n == 1 ? Mark::Present
                  : Mark::Empty);
  localwordle::Record fallback;
  fallback.played = model.stats.played;
  fallback.won = model.stats.won;
  fallback.streak = model.stats.streak;
  fallback.maximum = model.stats.streak;
  const auto& record = model.record ? *model.record : fallback;
  const char* names[] = {tr(STR_WORDLE_PLAYED), tr(STR_WORDLE_WIN_PERCENT), tr(STR_GUIDE_WORDLE_STREAK),
                         tr(STR_WORDLE_MAX_STREAK)};
  const uint32_t values[] = {record.played, record.played ? uint32_t(uint64_t(record.won) * 100 / record.played) : 0,
                             record.streak, record.maximum};
  const int statY = main.y + tile + 20, statH = 104;
  target.stroke(fui::makeRect(main.x, statY, main.width, statH), ink, pixelui::kBorder);
  for (int n = 0; n < 4; ++n) {
    const auto box = fui::makeRect(main.x + n * main.width / 4, statY, main.width / 4, statH);
    if (n) target.fill(fui::makeRect(box.x, box.y, 2, box.height), ink);
    std::snprintf(work.label, sizeof(work.label), n == 1 ? "%lu%%" : "%lu", static_cast<unsigned long>(values[n]));
    numerals::draw(target, fui::makeRect(box.x + 12, box.y + 12, box.width - 24, 38), work.label);
    char copy[64];
    pixelui::copyCaps(copy, sizeof(copy), names[n]);
    fui::TextStyle caption;
    caption.font = 3;
    caption.align = fui::TextAlign::Center;
    caption.maxLines = 2;
    target.text(fui::makeRect(box.x + 4, box.y + 56, box.width - 8, 42), copy, caption);
  }
  const auto right =
      wide ? fui::makeRect(main.right() + 16, body.y, body.right() - main.right() - 16, body.height) : body;
  const int controlsH = wide ? 52 : 60, controlsY = body.bottom() - 3 * controlsH - 16;
  const int chartHeading = wide ? right.y : statY + statH + 20;
  gamepanel::heading(screen, fui::makeRect(right.x, chartHeading, right.width, 28),
                     tr(STR_REDESIGN_GUESS_DISTRIBUTION));
  const int chartTop = chartHeading + 34;
  const int chartH = std::max(18, std::min(38, (controlsY - chartTop - 12) / 6));
  uint32_t most = 1;
  for (auto count : record.distribution) most = std::max(most, count);
  for (int n = 0; n < 6; ++n) {
    const int y = chartTop + n * chartH;
    std::snprintf(work.label, sizeof(work.label), "%d", n + 1);
    text(target, fui::makeRect(right.x, y, 28, chartH), work.label, 3);
    const int maximum = right.width - 88, width = int(uint64_t(maximum) * record.distribution[n] / most);
    const auto bar = fui::makeRect(right.x + 32, y + 3, maximum, chartH - 6);
    target.stroke(bar, ink, 2);
    if (width) target.fill(fui::makeRect(bar.x, bar.y, width, bar.height), ink);
    std::snprintf(work.label, sizeof(work.label), "%lu", static_cast<unsigned long>(record.distribution[n]));
    text(target, fui::makeRect(bar.right() + 4, y, 52, chartH), work.label, 3);
  }
  if (model.state && *model.state)
    text(target, fui::makeRect(right.x, controlsY - 30, right.width, 26), model.state, 3);
  gamepanel::row(screen, fui::makeRect(right.x, controlsY, right.width, controlsH), tr(STR_WORDLE_CONTINUE), ActionMenu,
                 0, gamepanel::Mark::Play, true);
  std::snprintf(work.label, sizeof(work.label), "%d", model.letters);
  gamepanel::row(screen, fui::makeRect(right.x, controlsY + controlsH + 8, right.width, controlsH),
                 tr(STR_WORDLE_LENGTH), ActionMenu, 2, gamepanel::Mark::Grid, false, work.label);
  gamepanel::row(screen, fui::makeRect(right.x, controlsY + 2 * (controlsH + 8), right.width, controlsH),
                 tr(STR_WORDLE_HELP), ActionMenu, 1, gamepanel::Mark::Help);
}
void buildLengths(toybox::Screen& screen, int selected, Work& work) {
  chrome(screen, tr(STR_WORDLE_NAME));
  dashboard::text(screen, screen.takeTop(48, 0), tr(STR_WORDLE_LENGTH), pixelui::kBody);
  dashboard::text(screen, screen.takeTop(32, 16), tr(STR_WORDLE_CHOOSE), 0);
  dashboard::button(screen, screen.takeBottom(62, 0), tr(STR_GUIDE_APPLY), ActionApplyLength, 0, true, &extra_check_32,
                    pixelui::kBody);
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  const int gap = 14, w = wide ? (body.width - 2 * gap) / 3 : body.width,
            h = wide ? body.height : std::min(128, (int(body.height) - 2 * gap) / 3);
  for (int n = 4; n <= 6; ++n) {
    const bool active = screen.frame().buttonSelected(ActionLength, n, n == selected);
    const auto box =
        fui::makeRect(body.x + (wide ? (n - 4) * (w + gap) : 0), body.y + (wide ? 0 : (n - 4) * (h + gap)), w, h);
    if (active)
      screen.target().fill(box, ink);
    else
      screen.target().stroke(box, ink, 4);
    screen.frame().hit(box, ActionLength, n, fui::InputTouch);
    const auto paint = active ? dashboard::paper : dashboard::ink;
    const int side = std::max(16, std::min(40, (int(box.width) - 100) / n)), pitch = side + 6;
    const int x = box.x + (box.width - 52 - n * pitch + 6) / 2, y = box.y + 20;
    for (int k = 0; k < n; ++k) {
      const auto tile = fui::makeRect(x + k * pitch, y, side, side);
      if (active)
        screen.target().fill(tile, paint);
      else
        screen.target().stroke(tile, paint, 3);
    }
    std::snprintf(work.label, sizeof(work.label), tr(STR_WORDLE_LETTERS), n);
    dashboard::text(screen, fui::makeRect(box.x + 12, y + side + 10, box.width - 72, 38), work.label, 0,
                    fui::TextAlign::Center, active);
    const auto check = fui::makeRect(box.right() - 46, box.y + (box.height - 30) / 2, 30, 30);
    if (active) {
      screen.target().fill(check, paint);
      screen.target().fill(check.inset(fui::Insets{7, 7, 7, 7}), dashboard::ink);
    } else
      screen.target().stroke(check, paint, 3);
  }
}
void buildConfirm(toybox::Screen& screen, Work&) {
  chrome(screen, tr(STR_WORDLE_NEW));
  const auto body = screen.body();
  text(screen.target(), fui::makeRect(body.x + 12, body.y + 24, body.width - 24, 160), tr(STR_WORDLE_REPLACE_NOTICE),
       pixelui::kSmall, 4);
  button(screen, fui::makeRect(body.x, body.bottom() - 136, body.width, 60), tr(STR_WORDLE_START_NEW), ActionConfirm);
  button(screen, fui::makeRect(body.x, body.bottom() - 64, body.width, 60), tr(STR_WORDLE_CANCEL), ActionArchive);
}
void buildHowTo(toybox::Screen& screen) {
  chrome(screen, tr(STR_WORDLE_HELP));
  dashboard::text(screen, screen.takeTop(42, 8), tr(STR_GUIDE_WORDLE_GOAL), 0);
  dashboard::button(screen, screen.takeBottom(54, 12), tr(STR_UI_GOT_IT_C3A894), ActionArchive, 0, true, nullptr,
                    pixelui::kBody);
  dashboard::text(screen, screen.takeBottom(30, 8), tr(STR_GUIDE_WORDLE_REPEATED), 3);
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  const int gap = 12, w = wide ? (body.width - gap) / 2 : body.width;
  const int lessonH = wide ? (body.height - 2 * gap) / 3 : std::min(112, (int(body.height) - 156) / 3);
  static constexpr StrId titles[] = {StrId::STR_GUIDE_WORDLE_CORRECT, StrId::STR_GUIDE_WORDLE_PRESENT,
                                     StrId::STR_GUIDE_WORDLE_ABSENT};
  static constexpr StrId copy[] = {StrId::STR_GUIDE_WORDLE_CORRECT_BODY, StrId::STR_GUIDE_WORDLE_PRESENT_BODY,
                                   StrId::STR_GUIDE_WORDLE_ABSENT_BODY};
  static constexpr char letters[] = {'A', 'R', 'S'};
  static constexpr Mark marks[] = {Mark::Correct, Mark::Present, Mark::Absent};
  for (int n = 0; n < 3; ++n) {
    const auto row = fui::makeRect(body.x, body.y + n * (lessonH + gap), w, lessonH);
    const int side = std::min(86, lessonH - 8);
    cell(screen.target(), fui::makeRect(row.x + 4, row.y + 4, side, side), letters[n], marks[n]);
    dashboard::text(screen, fui::makeRect(row.x + side + 18, row.y + 4, row.width - side - 20, 34), I18N.get(titles[n]),
                    0, fui::TextAlign::Left);
    guideui::text(screen, fui::makeRect(row.x + side + 18, row.y + 42, row.width - side - 20, row.height - 42),
                  I18N.get(copy[n]), 3, fui::TextAlign::Left, 4);
  }
  const auto note =
      wide ? fui::makeRect(body.x + w + gap, body.y, w, body.height)
           : fui::makeRect(body.x, body.y + 3 * (lessonH + gap), body.width, body.height - 3 * (lessonH + gap));
  const int h = (note.height - 16) / 3;
  static constexpr StrId notices[] = {StrId::STR_GUIDE_WORDLE_SUBMIT, StrId::STR_GUIDE_WORDLE_INVALID,
                                      StrId::STR_GUIDE_WORDLE_ACCENTS};
  for (int n = 0; n < 3; ++n) {
    const auto r = fui::makeRect(note.x, note.y + n * (h + 8), note.width, h);
    dashboard::frame(screen, r, 3);
    if (n == 0)
      dashboard::icon(screen, fui::makeRect(r.x + 10, r.y + (h - 30) / 2, 30, 30), extra_check_32);
    else if (n == 1) {
      screen.target().stroke(fui::makeRect(r.x + 10, r.y + (h - 30) / 2, 30, 30), ink, 3, 15);
      screen.target().line({int16_t(r.x + 14), int16_t(r.y + (h - 30) / 2 + 4)},
                           {int16_t(r.x + 36), int16_t(r.y + (h - 30) / 2 + 26)}, 4, ink);
    } else
      guideui::grid(screen, fui::makeRect(r.x + 10, r.y + (h - 30) / 2, 30, 30), 3);
    guideui::text(screen, fui::makeRect(r.x + 52, r.y + 6, r.width - 64, h - 12), I18N.get(notices[n]), 2,
                  fui::TextAlign::Left, 4);
  }
}
}  // namespace wordleui
