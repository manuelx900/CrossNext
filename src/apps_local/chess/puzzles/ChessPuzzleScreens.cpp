#include "ChessPuzzleScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../../ui/CompactArt.h"
#include "../../ui/Dashboard.h"
#include "../../ui/GameFrontDoor.h"
#include "../../ui/PageIndicator.h"
#include "../../ui/PixelMenu.h"
#include "../../ui/ReferenceArt.h"
#include "../../ui/ToyboxIcons.h"
#include "../ChessPieces.h"
#include "../ChessStyle.h"

namespace chesspuzzleui {
namespace {
namespace fui = freeink::ui;
const auto black = fui::Paint::solid(fui::Color::Black);
const char* difficulty(int group) {
  static constexpr StrId keys[] = {StrId::STR_CHESS_PUZZLE_EASY, StrId::STR_CHESS_PUZZLE_MEDIUM,
                                   StrId::STR_CHESS_PUZZLE_HARD};
  return I18N.get(keys[group]);
}
void button(toybox::Screen& screen, Work& work, fui::Rect rect, const char* label, fui::ActionId action,
            int value = 0) {
  work.button = {};
  auto& props = work.button;
  pixelui::copyCaps(work.buttonLabel, sizeof(work.buttonLabel), label);
  props.label = work.buttonLabel;
  props.text.font = pixelui::kSmall;
  props.text.align = fui::TextAlign::Center;
  props.styles = pixelui::rowStyles();
  props.action = action;
  props.value = value;
  props.minTouchSize = 0;
  screen.button(props, rect);
}

void navigation(toybox::Screen& screen, Work& work, fui::Rect rect, const char* label, fui::ActionId action) {
  button(screen, work, rect, "", action);
  const bool inverse = screen.frame().buttonSelected(action, 0);
  const auto ink = fui::Paint::solid(inverse ? fui::Color::White : fui::Color::Black);
  auto text = rect.inset(fui::Insets{8, 8, 8, 8});
  const int side = action == Restart ? 32 : 22;
  if (action == Choose || action == Restart) {
    gamepanel::mark(screen, fui::makeRect(rect.x + 10, rect.y + (rect.height - side) / 2, side, side),
                    action == Choose ? gamepanel::Mark::Grid : gamepanel::Mark::Restart, inverse);
    text.x += side + 8;
    text.width -= side + 8;
  } else if (action == Previous || action == Next) {
    const int x = action == Previous ? rect.x + 10 : rect.right() - 10;
    const int direction = action == Previous ? 1 : -1, y = rect.y + rect.height / 2;
    screen.target().line({int16_t(x + direction * 8), int16_t(y - 10)}, {int16_t(x), int16_t(y)}, 2, ink);
    screen.target().line({int16_t(x), int16_t(y)}, {int16_t(x + direction * 8), int16_t(y + 10)}, 2, ink);
    if (action == Previous) text.x += 20;
    text.width -= 20;
  }
  pixelui::copyCaps(work.buttonLabel, sizeof(work.buttonLabel), label);
  int font = 0;
  if (action == Previous || action == Next) {
    fui::TextStyle probe;
    probe.font = 0;
    char previous[48], next[48];
    pixelui::copyCaps(previous, sizeof(previous), tr(STR_READING_PREV));
    pixelui::copyCaps(next, sizeof(next), tr(STR_READING_NEXT));
    if (screen.target().measureText(0, previous, probe).width > text.width ||
        screen.target().measureText(0, next, probe).width > text.width)
      font = 3;
  }
  dashboard::text(screen, text, work.buttonLabel, font, fui::TextAlign::Center, inverse);
}

const char* status(chesspuzzle::Status value) {
  switch (value) {
    case chesspuzzle::Status::Playing:
      return tr(STR_CHESS_PUZZLE_PLAY);
    case chesspuzzle::Status::Correct:
      return tr(STR_CHESS_PUZZLE_CORRECT);
    case chesspuzzle::Status::Thinking:
      return tr(STR_CHESS_PUZZLE_THINKING);
    case chesspuzzle::Status::Lost:
      return tr(STR_CHESS_PUZZLE_LOST);
    case chesspuzzle::Status::Draw:
      return tr(STR_CHESS_PUZZLE_DRAW);
    case chesspuzzle::Status::Illegal:
      return tr(STR_CHESS_PUZZLE_ILLEGAL);
    case chesspuzzle::Status::Solved:
      return tr(STR_CHESS_PUZZLE_SOLVED);
    case chesspuzzle::Status::DataError:
      return tr(STR_CHESS_PUZZLE_DATA_ERROR);
  }
  return "";
}
}  // namespace
fui::Rect boardRect(const fui::DeviceContext& device) {
  const auto body = pixelui::geometry(device).body;
  const bool wide = body.width > body.height;
  const int size =
      std::max(8, std::min(wide ? body.width - 228 : body.width - 8, body.height - (wide ? 8 : 250))) / 8 * 8;
  return fui::makeRect(body.x + (wide ? 4 : (body.width - size) / 2), body.y + (wide ? 4 : 76), size, size);
}
int squareAt(const fui::DeviceContext& device, bool white, int x, int y) {
  const auto board = boardRect(device);
  if (x < board.x || y < board.y || x >= board.right() || y >= board.bottom()) return -1;
  const int col = (x - board.x) * 8 / board.width, row = (y - board.y) * 8 / board.height;
  return white ? (7 - row) * 8 + col : row * 8 + 7 - col;
}
void build(toybox::Screen& screen, const Model& model, Work& work) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_CHESS_PUZZLES);
  header.back = screen.device().hasTouch;
  pixelui::header(screen, header);
  backui::publish(header.back, pixelui::geometry(screen.device()).back);
  const auto body = screen.body();
  if (model.view == View::Difficulty) {
    const bool wide = body.width > body.height;
    const int total = chesspuzzle::solvedCount(*model.progress, 0) + chesspuzzle::solvedCount(*model.progress, 1) +
                      chesspuzzle::solvedCount(*model.progress, 2);
    const int heroWidth = wide ? body.width * 2 / 5 : body.width;
    const auto hero = fui::makeRect(body.x, body.y, heroWidth, wide ? 112 : 86);
    dashboard::frame(screen, hero, 2);
    compactart::draw(screen, fui::makeRect(hero.x + 16, hero.y + 10, 48, hero.height - 20), compactart::PuzzleKnight);
    pixelui::text(screen.target(),
                  fui::makeRect(hero.x + 86, hero.y + 8, wide ? hero.width - 102 : hero.width - 222,
                                wide ? 42 : hero.height - 16),
                  tr(STR_CHESS_CHALLENGES_SOLVED), 3);
    std::snprintf(work.labels[0], sizeof(work.labels[0]), "%d / %d", total, chesspuzzle::kCount);
    dashboard::digits(screen,
                      fui::makeRect(wide ? hero.x + 86 : hero.right() - 146, hero.y + (wide ? 64 : 24),
                                    wide ? hero.width - 102 : 130, 36),
                      work.labels[0]);
    const int left = wide ? hero.right() + 18 : body.x, width = body.right() - left;
    int y = wide ? body.y : hero.bottom() + 16;
    if (model.invalidNumber || model.saveFailed) {
      frontdoor::text(screen, fui::makeRect(left, y, width, 28),
                      model.saveFailed ? tr(STR_CHESS_PUZZLE_SAVE_ERROR) : tr(STR_CHESS_PUZZLE_BAD_NUMBER), 3, false);
      y += 32;
    }
    pixelui::text(screen.target(), fui::makeRect(left, y, width, 34), tr(STR_CHESS_PUZZLE_CHOOSE), 0);
    y += 40;
    const int gap = wide ? 8 : 14, height = wide ? 72 : 94;
    for (int n = 0; n < 3; ++n) {
      const auto row = fui::makeRect(left, y + n * (height + gap), width, height);
      dashboard::frame(screen, row, 2);
      for (int bar = 0; bar <= n + (n == 0 ? 1 : 0); ++bar)
        screen.target().fill(fui::makeRect(row.x + 14 + bar * 14, row.y + height / 2 - 8 - bar * 6, 11, 26 + bar * 6),
                             black);
      pixelui::text(screen.target(), fui::makeRect(row.x + 64, row.y + 8, row.width - 210, row.height - 16),
                    difficulty(n), 1);
      const int solved = chesspuzzle::solvedCount(*model.progress, n);
      std::snprintf(work.labels[n], sizeof(work.labels[n]), "%d / %d", solved, chesspuzzle::kGroupCounts[n]);
      dashboard::digits(screen, fui::makeRect(row.right() - 136, row.y + 16, 100, 28), work.labels[n]);
      dashboard::text(screen, fui::makeRect(row.right() - 138, row.y + 48, 104, 26), tr(STR_CHESS_SOLVED_LABEL), 3);
      dashboard::chevron(screen.target(), fui::makeRect(row.right() - 32, row.y, 24, height));
      screen.frame().hit(row, Difficulty, n, fui::InputDefault);
    }
    const int artTop = wide ? hero.bottom() + 16 : y + 3 * (height + gap);
    const auto art = wide ? fui::makeRect(hero.x, artTop, hero.width, body.bottom() - 76 - artTop)
                          : fui::makeRect(body.x, artTop, body.width, body.bottom() - 86 - artTop);
    compactart::draw(screen, art, compactart::PuzzleBoard);
    gamepanel::row(screen, fui::makeRect(body.x, body.bottom() - 70, wide ? heroWidth : body.width, 70),
                   tr(STR_CHESS_PUZZLE_RESET), Reset, 0, gamepanel::Mark::Restart, false, nullptr, false);
    return;
  }
  if (model.view == View::Select) {
    const bool wide = body.width > body.height;
    const int count = chesspuzzle::kGroupCounts[model.group], start = chesspuzzle::kGroupStarts[model.group];
    const int solved = chesspuzzle::solvedCount(*model.progress, model.group);
    auto title = screen.takeTop(wide ? 30 : 38, wide ? 6 : 10);
    pixelui::text(screen.target(), fui::makeRect(title.x, title.y, title.width * 3 / 4, title.height),
                  tr(STR_CHESS_SELECT_PROBLEM), pixelui::kSmall);
    pixelui::text(screen.target(), fui::makeRect(title.x + title.width * 3 / 4, title.y, title.width / 4, title.height),
                  difficulty(model.group), 3, fui::Color::Black, fui::TextAlign::Right);
    const int last = model.progress->lastIndex;
    const bool resume = last >= start && last < start + count && !chesspuzzle::solved(*model.progress, last);
    if (resume) {
      const auto r = screen.takeTop(wide ? 54 : 82, wide ? 8 : 12);
      std::snprintf(work.labels[0], sizeof(work.labels[0]), tr(STR_CHESS_PUZZLE_RESUME), last - start + 1);
      gamepanel::row(screen, r, wide ? work.labels[0] : "", Resume, 0, gamepanel::Mark::Play, true);
      if (!wide)
        pixelui::text(screen.target(), fui::makeRect(r.x + 64, r.y + 10, r.width - 110, 32), work.labels[0],
                      pixelui::kSmall, fui::Color::White);
      if (!wide) {
        screen.target().fill(fui::makeRect(r.x + 64, r.y + 43, r.width - 110, 30), black);
        pixelui::text(screen.target(), fui::makeRect(r.x + 64, r.y + 43, r.width - 110, 30),
                      tr(STR_CHESS_PROBLEM_PENDING), 3, fui::Color::White);
      }
    }
    const auto summary = screen.takeTop(wide ? 28 : 32, 4);
    std::snprintf(work.labels[0], sizeof(work.labels[0]), tr(STR_CHESS_PUZZLE_GROUP_PROGRESS), solved, count);
    pixelui::text(screen.target(), summary, work.labels[0], pixelui::kSmall);
    dashboard::bar(screen, screen.takeTop(wide ? 10 : 12, wide ? 8 : 16), solved, count);
    if (!wide) {
      auto legend = screen.takeBottom(30, 4);
      dashboard::icon(screen, fui::makeRect(legend.x + 18, legend.y + 4, 22, 22), extra_check_32);
      pixelui::text(screen.target(), fui::makeRect(legend.x + 50, legend.y, legend.width / 2 - 50, 30),
                    tr(STR_TASK_COMPLETED), 3);
      screen.target().stroke(fui::makeRect(legend.x + legend.width / 2, legend.y + 4, 22, 22), black, 2);
      pixelui::text(screen.target(),
                    fui::makeRect(legend.x + legend.width / 2 + 34, legend.y, legend.width / 2 - 34, 30),
                    tr(STR_CHESS_PROBLEM_PENDING), 3);
    }
    const int pages = (count + 15) / 16, page = std::clamp(model.page, 0, pages - 1);
    pageui::build(screen, screen.takeBottom(wide ? 44 : 48, wide ? 8 : 12), page + 1, pages, Page, -1, Page, 1);
    const auto grid = screen.body();
    const int gap = 10, cols = wide ? 8 : 4, rows = wide ? 2 : 4;
    const int w = (grid.width - gap * (cols - 1)) / cols, h = (grid.height - gap * (rows - 1)) / rows;
    for (int n = 0; n < 16 && page * 16 + n < count; ++n) {
      const int local = page * 16 + n, index = start + local;
      const auto r = fui::makeRect(grid.x + n % cols * (w + gap), grid.y + n / cols * (h + gap), w, h);
      const bool selected =
          screen.frame().buttonSelected(Pick, index, index == last || chesspuzzle::solved(*model.progress, index));
      if (selected)
        screen.target().fill(r, black);
      else
        screen.target().stroke(r, black, 2);
      std::snprintf(work.labels[0], sizeof(work.labels[0]), "%d", local + 1);
      numerals::draw(screen.target(),
                     r.inset(fui::Insets{int16_t(std::max(8, h / 4)), 12, int16_t(std::max(8, h / 4)), 12}),
                     work.labels[0], selected ? fui::Color::White : fui::Color::Black);
      if (chesspuzzle::solved(*model.progress, index))
        dashboard::icon(screen, fui::makeRect(r.right() - 24, r.y + 4, 20, 20), extra_check_32, selected);
      screen.frame().hit(r, Pick, index, fui::InputTouch);
    }
    return;
  }
  if (model.view == View::ConfirmReset) {
    fui::TextStyle text;
    text.font = pixelui::kBody;
    text.maxLines = 4;
    screen.target().text(fui::makeRect(body.x + 12, body.y + 24, body.width - 24, body.height / 2),
                         tr(STR_CHESS_PUZZLE_RESET_ASK), text);
    button(screen, work, fui::makeRect(body.x, body.bottom() - 132, body.width, 60), tr(STR_CHESS_PUZZLE_ERASE),
           ConfirmReset);
    button(screen, work, fui::makeRect(body.x, body.bottom() - 60, body.width, 60), tr(STR_CANCEL), Cancel);
    return;
  }
  if (model.view == View::Promotion) {
    pixelui::text(screen.target(), fui::makeRect(body.x, body.y, body.width, 44), tr(STR_CHESS_PUZZLE_PROMOTE),
                  pixelui::kBody);
    static constexpr UiLabel names[] = {trLabel(STR_UI_QUEEN_6911AD), trLabel(STR_UI_ROOK_B2A943),
                                        trLabel(STR_UI_BISHOP_D4A7B6), trLabel(STR_UI_KNIGHT_2352A2)};
    static constexpr int types[] = {chess::Queen, chess::Rook, chess::Bishop, chess::Knight};
    for (int n = 0; n < 4; ++n)
      button(screen, work, fui::makeRect(body.x, body.y + 52 + n * 60, body.width, 60), names[n], Promote, types[n]);
    return;
  }
  if (model.view == View::Celebration) {
    const bool allComplete = chesspuzzle::solvedCount(*model.progress, 0) +
                                 chesspuzzle::solvedCount(*model.progress, 1) +
                                 chesspuzzle::solvedCount(*model.progress, 2) ==
                             chesspuzzle::kCount;
    const char* completion = allComplete ? tr(STR_CHESS_PUZZLE_ALL) : tr(STR_CHESS_PROBLEM_FINISHED);
    const bool wide = body.width > body.height;
    if (wide) {
      const int left = body.width * 2 / 5;
      const auto info = fui::makeRect(body.x + left + 16, body.y, body.width - left - 16, body.height);
      referenceart::draw(screen, fui::makeRect(body.x + (left - 120) / 2, body.y + 4, 120, 156),
                         referenceart::KnightTrophy);
      fui::TextStyle caption;
      caption.font = pixelui::kBody;
      caption.align = fui::TextAlign::Center;
      caption.maxLines = 2;
      screen.target().text(fui::makeRect(body.x, body.y + 152, left, 76), completion, caption);
      pixelui::text(screen.target(), fui::makeRect(body.x, body.y + 236, left, 40), tr(STR_CHESS_WELL_PLAYED),
                    pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
      std::snprintf(work.labels[0], sizeof(work.labels[0]), tr(STR_CHESS_PUZZLE_MOVES), model.session->playerMoves,
                    chesspuzzle::goal(*model.session));
      dashboard::frame(screen, fui::makeRect(info.x, info.y, info.width, 48), 2);
      pixelui::text(screen.target(), fui::makeRect(info.x + 10, info.y, info.width - 20, 48), work.labels[0],
                    pixelui::kSmall);
      const int solved = chesspuzzle::solvedCount(*model.progress, model.group),
                count = chesspuzzle::kGroupCounts[model.group];
      dashboard::bar(screen, fui::makeRect(info.x, info.y + 60, info.width, 16), solved, count);
      std::snprintf(work.labels[0], sizeof(work.labels[0]), tr(STR_CHESS_PUZZLE_GROUP_PROGRESS), solved, count);
      pixelui::text(screen.target(), fui::makeRect(info.x, info.y + 82, info.width, 32), work.labels[0], 3,
                    fui::Color::Black, fui::TextAlign::Center);
      const int h = 48, y = info.bottom() - 3 * h - 16;
      gamepanel::row(screen, fui::makeRect(info.x, y, info.width, h), tr(STR_CHESS_PROBLEM_NEXT), Next, 0,
                     gamepanel::Mark::Flag, true);
      gamepanel::row(screen, fui::makeRect(info.x, y + h + 8, info.width, h), tr(STR_CHESS_PUZZLE_REVIEW), Review, 0,
                     gamepanel::Mark::Grid);
      gamepanel::row(screen, fui::makeRect(info.x, y + 2 * (h + 8), info.width, h), tr(STR_CHESS_PUZZLE_JUMP), Choose,
                     0, gamepanel::Mark::Grid);
      return;
    }
    const int cx = body.x + body.width / 2, y = body.y + 8;
    referenceart::draw(screen, fui::makeRect(cx - 64, y, 128, 166), referenceart::KnightTrophy);
    const auto badge = fui::makeRect(cx - 64, y + 148, 128, 18);
    const int headingY = badge.bottom() + 12;
    pixelui::text(screen.target(), fui::makeRect(body.x, headingY, body.width, 42), completion, pixelui::kBody,
                  fui::Color::Black, fui::TextAlign::Center);
    std::snprintf(work.labels[0], sizeof(work.labels[0]), "%s · %d", difficulty(model.group),
                  model.index - chesspuzzle::kGroupStarts[model.group] + 1);
    pixelui::text(screen.target(), fui::makeRect(body.x, headingY + 44, body.width, 32), work.labels[0],
                  pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
    pixelui::text(screen.target(), fui::makeRect(body.x, headingY + 80, body.width, 32), tr(STR_CHESS_WELL_PLAYED),
                  pixelui::kSmall, fui::Color::Black, fui::TextAlign::Center);
    const int solved = chesspuzzle::solvedCount(*model.progress, model.group),
              count = chesspuzzle::kGroupCounts[model.group];
    const int rowH = 54, top = headingY + 122;
    std::snprintf(
        work.labels[0], sizeof(work.labels[0]), tr(STR_CHESS_PUZZLE_OBJECTIVE),
        model.session->source && model.session->source->white ? tr(STR_CHESS_PUZZLE_WHITE) : tr(STR_CHESS_PUZZLE_BLACK),
        chesspuzzle::goal(*model.session));
    dashboard::frame(screen, fui::makeRect(body.x, top, body.width, rowH), 2);
    pixelui::text(screen.target(), fui::makeRect(body.x + 16, top, body.width - 32, rowH), work.labels[0],
                  pixelui::kSmall);
    std::snprintf(work.labels[0], sizeof(work.labels[0]), tr(STR_CHESS_PUZZLE_MOVES), model.session->playerMoves,
                  chesspuzzle::goal(*model.session));
    dashboard::frame(screen, fui::makeRect(body.x, top + rowH, body.width, rowH), 2);
    pixelui::text(screen.target(), fui::makeRect(body.x + 16, top + rowH, body.width - 32, rowH), work.labels[0],
                  pixelui::kSmall);
    dashboard::bar(screen, fui::makeRect(body.x, top + 2 * rowH + 14, body.width, 16), solved, count);
    std::snprintf(work.labels[0], sizeof(work.labels[0]), tr(STR_CHESS_PUZZLE_GROUP_PROGRESS), solved, count);
    pixelui::text(screen.target(), fui::makeRect(body.x, top + 2 * rowH + 34, body.width, 30), work.labels[0], 3,
                  fui::Color::Black, fui::TextAlign::Center);
    const int h = wide ? 44 : 58, start = body.bottom() - 3 * h - 20;
    gamepanel::row(screen, fui::makeRect(body.x, start, body.width, h), tr(STR_CHESS_PROBLEM_NEXT), Next, 0,
                   gamepanel::Mark::Flag, true);
    gamepanel::row(screen, fui::makeRect(body.x, start + h + 10, body.width, h), tr(STR_CHESS_PUZZLE_REVIEW), Review, 0,
                   gamepanel::Mark::Grid);
    gamepanel::row(screen, fui::makeRect(body.x, start + 2 * (h + 10), body.width, h), tr(STR_CHESS_PUZZLE_JUMP),
                   Choose, 0, gamepanel::Mark::Grid);
    return;
  }
  const auto& session = *model.session;
  if (!session.source) return;
  const auto board = boardRect(screen.device());
  const bool wide = body.width > body.height;
  const auto info = wide ? fui::makeRect(board.right() + 16, body.y, body.right() - board.right() - 16, body.height)
                         : fui::makeRect(body.x, body.y, body.width, 68);
  std::snprintf(work.labels[0], sizeof(work.labels[0]), "%s  #%d / 1000", difficulty(model.group), model.index + 1);
  std::snprintf(work.labels[1], sizeof(work.labels[1]), tr(STR_CHESS_PUZZLE_OBJECTIVE),
                session.source->white ? tr(STR_CHESS_PUZZLE_WHITE) : tr(STR_CHESS_PUZZLE_BLACK),
                (session.source->plies + 1) / 2);
  std::snprintf(work.labels[2], sizeof(work.labels[2]), tr(STR_CHESS_PUZZLE_REMAINING),
                chesspuzzle::remaining(session));
  std::snprintf(work.labels[3], sizeof(work.labels[3]), tr(STR_CHESS_PUZZLE_RATING), session.source->rating);
  std::snprintf(work.labels[4], sizeof(work.labels[4]), tr(STR_CHESS_PUZZLE_MOVES), session.playerMoves,
                chesspuzzle::goal(session));
  const auto band = fui::makeRect(info.x, info.y, info.width, 38);
  screen.target().stroke(band, black, 2);
  const int half = band.width / 2;
  if (!wide)
    screen.target().line({int16_t(band.x + half), band.y}, {int16_t(band.x + half), int16_t(band.bottom())}, 2, black);
  pixelui::text(screen.target(), fui::makeRect(band.x + 6, band.y + 4, wide ? band.width - 12 : half - 12, 30),
                work.labels[0], pixelui::kSmall);
  pixelui::text(screen.target(),
                fui::makeRect(wide ? band.x + 6 : band.x + half + 6, wide ? band.bottom() + 4 : band.y + 4,
                              wide ? band.width - 12 : band.width - half - 12, 30),
                work.labels[4], pixelui::kSmall, fui::Color::Black, fui::TextAlign::Right);
  pixelui::text(screen.target(), fui::makeRect(info.x, info.y + (wide ? 78 : 42), info.width, 30), work.labels[1],
                pixelui::kSmall);

  const int square = board.width / 8;
  for (int n = 0; n < 64; ++n) {
    const int column = session.source->white ? n % 8 : 7 - n % 8;
    const int row = session.source->white ? 7 - n / 8 : n / 8;
    const auto cell = fui::makeRect(board.x + column * square, board.y + row * square, square, square);
    if ((n / 8 + n % 8) % 2 == 0) screen.target().fill(cell, fui::Paint::solid(fui::Color::LightGray));
    const auto piece = session.position.squares[n];
    if (piece) {
      const int size = std::min(chessart::kPieceSize, square - 6);
      const auto box = fui::makeRect(cell.x + (square - size) / 2, cell.y + (square - size) / 2, size, size);
      chessstyle::draw(screen.target(), box, chess::pieceType(piece), chess::isBlack(piece), model.pieceDesign);
    }
    if (n == session.lastFrom || n == session.lastTo)
      screen.target().stroke(fui::makeRect(cell.x + 2, cell.y + 2, cell.width - 4, cell.height - 4), black, 1);
    if (n == session.selected)
      screen.target().stroke(fui::makeRect(cell.x + 2, cell.y + 2, cell.width - 4, cell.height - 4), black, 4);
  }
  screen.target().stroke(board, black, pixelui::kBorder);
  const auto feedback = wide ? fui::makeRect(info.x, info.y + 146, info.width, 64)
                             : fui::makeRect(body.x, board.bottom() + 6, body.width, 52);
  pixelui::text(screen.target(), fui::makeRect(feedback.x, feedback.y, feedback.width, 28), status(session.status),
                pixelui::kSmall);
  pixelui::text(screen.target(), fui::makeRect(feedback.x, feedback.y + 28, feedback.width, 24),
                model.invalidNumber ? tr(STR_CHESS_PUZZLE_BAD_NUMBER)
                : model.saveFailed  ? tr(STR_CHESS_PUZZLE_SAVE_ERROR)
                                    : work.labels[2],
                3);
  const auto controls = wide ? fui::makeRect(info.x, body.bottom() - 112, info.width, 112)
                             : fui::makeRect(body.x, body.bottom() - 110, body.width, 110);
  if (wide) {
    const int gap = 8, w = (controls.width - gap * 2) / 3;
    navigation(screen, work, fui::makeRect(controls.x, controls.y, w, 48), "", Previous);
    std::snprintf(work.labels[3], sizeof(work.labels[3]), "#%d", model.index + 1);
    navigation(screen, work, fui::makeRect(controls.x + w + gap, controls.y, w, 48), work.labels[3], Choose);
    navigation(screen, work, fui::makeRect(controls.x + (w + gap) * 2, controls.y, controls.width - (w + gap) * 2, 48),
               "", Next);
    navigation(screen, work, fui::makeRect(controls.x, controls.y + 60, controls.width, 52),
               tr(STR_CHESS_PUZZLE_RESTART), Restart);
  } else {
    const int gap = 8, w = (controls.width - gap * 2) / 3;
    navigation(screen, work, fui::makeRect(controls.x, controls.y, w, 46), tr(STR_READING_PREV), Previous);
    std::snprintf(work.labels[3], sizeof(work.labels[3]), "#%d", model.index + 1);
    navigation(screen, work, fui::makeRect(controls.x + w + gap, controls.y, w, 46), work.labels[3], Choose);
    navigation(screen, work, fui::makeRect(controls.x + (w + gap) * 2, controls.y, controls.width - (w + gap) * 2, 46),
               tr(STR_READING_NEXT), Next);
    navigation(screen, work, fui::makeRect(controls.x, controls.y + 56, controls.width, 54),
               tr(STR_CHESS_PUZZLE_RESTART), Restart);
  }
}
}  // namespace chesspuzzleui
