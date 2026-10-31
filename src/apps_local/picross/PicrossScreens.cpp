#include "PicrossScreens.h"

#include <I18n.h>

#include <cstdio>

#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/GameFrontDoor.h"
#include "../ui/GameGuide.h"
#include "../ui/PageIndicator.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxText.h"

namespace picrossui {

namespace {

// snprintf buffer sizes derived from their formats, not guessed: snprintf cuts
// rather than overruns, so a byte-short buffer is a silently shortened label.
// See ToyboxFormat.h.
constexpr int kMistakeChars = toybox::kIntChars + toybox::literalChars("MISTAKES  ") + 1;
constexpr int kGradeChars =
    toybox::kIntChars + toybox::literalChars("SOLVED WITH ") + toybox::literalChars(" MISTAKES") + 1;

// The strip under the header carrying the mistake count. The puzzle NAME is
// deliberately absent: the name is the answer, so showing it while you solve
// would spoil the picture. It is revealed on the win screen.
constexpr int kStatusStrip = 34;

// A cell never grows past this, so a 5x5 stays a board rather than becoming five
// enormous squares floating in the panel.
constexpr int kMaxCell = 68;

// Air between the clue gutter and the grid. The board frame is stroked OUTSIDE
// the play area (kBoardFrame wide), so without this the last clue in a row ran
// under the frame and the bottom column clue sat on row 0 -- clues bleeding into
// the playfield, which is exactly what Mario rejected. Reserve the frame plus a
// few pixels of visible separation, the way the dungeon board does.
constexpr int kClueGap = toybox::kBoardFrame + 6;

// Clue numbers are at most two digits ("10"), so the gutter slots are sized for
// that plus a little air. Two ladders: the 5x5 boards have room for the UI cut,
// the 10x10 boards use the tile cut so four numbers still fit the gutter.
struct ClueMetrics {
  fui::FontId font;
  toybox::CutMetrics cut;
  int16_t slotH;  // vertical pitch of a stacked column-clue number
  int16_t numW;   // horizontal width of a row-clue number cell
};

ClueMetrics clueMetricsFor(const int n) {
  if (n <= 5) return {toybox::kUiFont, toybox::kUiCut, 30, 28};
  return {3, toybox::kTileCut, 16, 18};
}

int maxRunsInRows(const picross::Board& board) {
  int most = 1;
  uint8_t buf[picross::kMaxSize];
  for (int r = 0; r < board.size(); ++r) {
    const int k = board.rowClues(r, buf);
    if (k > most) most = k;
  }
  return most;
}

int maxRunsInCols(const picross::Board& board) {
  int most = 1;
  uint8_t buf[picross::kMaxSize];
  for (int c = 0; c < board.size(); ++c) {
    const int k = board.colClues(c, buf);
    if (k > most) most = k;
  }
  return most;
}

void chrome(toybox::Screen& screen, const char* title, const char*) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.back = true;
  pixelui::header(screen, header);
  backui::publish(true, pixelui::geometry(screen.device()).back);
}

fui::Rect cellRect(const Layout& layout, const int row, const int col) {
  return fui::makeRect(static_cast<int16_t>(layout.board.x + col * layout.cell),
                       static_cast<int16_t>(layout.board.y + row * layout.cell), layout.cell, layout.cell);
}

// An X drawn as two diagonals. `pad` keeps it clear of the cell edge so it reads
// as a mark rather than as a filled corner; `weight` is thick enough that e-ink
// does not swallow it.
void drawX(toybox::Screen& screen, const fui::Rect& cell, const fui::Color color) {
  const int16_t pad = static_cast<int16_t>(cell.width / 4);
  const uint8_t weight = static_cast<uint8_t>(cell.width >= 44 ? 3 : 2);
  const fui::Paint paint = fui::Paint::solid(color);
  screen.target().line(fui::Point{static_cast<int16_t>(cell.x + pad), static_cast<int16_t>(cell.y + pad)},
                       fui::Point{static_cast<int16_t>(cell.right() - pad), static_cast<int16_t>(cell.bottom() - pad)},
                       weight, paint);
  screen.target().line(fui::Point{static_cast<int16_t>(cell.right() - pad), static_cast<int16_t>(cell.y + pad)},
                       fui::Point{static_cast<int16_t>(cell.x + pad), static_cast<int16_t>(cell.bottom() - pad)},
                       weight, paint);
}

// An asterisk: six spokes from the centre, a vertical and two diagonals. Drawn
// rather than set as a glyph, because the font's '*' sits high on the line (it
// is a superscript mark) and a cell wants it centred, and because at a 30-odd
// pixel cell the drawn one can be given a weight that survives e-ink.
//
// IT OCCUPIES EXACTLY THE X's BOX. Same reach (a quarter of the cell from the
// centre), same 2px weight, so the mistake and the player's own mark differ by
// GLYPH and by nothing else -- which is the distinction Mario drew, and the
// reason the mistake no longer needs weight to be told apart.
//
// The six arms are what separates it from the four-armed X at this size, and
// the arm ANGLES are the part that had to be looked at rather than reasoned
// about. The first version put the diagonals at 63 degrees from horizontal,
// which bunches all three strokes near the vertical: rendered at a 37px cell it
// read as a dense double-dagger, not as an asterisk. Six even arms means 60
// degrees apart, so the diagonals sit at 30 degrees from HORIZONTAL -- dx:dy of
// 7:4, close enough at this size and needing no trigonometry.
void drawAsterisk(toybox::Screen& screen, const fui::Rect& cell, const fui::Color color) {
  const int16_t cx = static_cast<int16_t>(cell.x + cell.width / 2);
  const int16_t cy = static_cast<int16_t>(cell.y + cell.height / 2);
  const int16_t r = static_cast<int16_t>(cell.width / 4);
  if (r <= 0) return;
  const uint8_t weight = static_cast<uint8_t>(cell.width >= 26 ? 2 : 1);
  const fui::Paint paint = fui::Paint::solid(color);
  const int16_t dx = static_cast<int16_t>(r * 7 / 8);
  const int16_t dy = static_cast<int16_t>(r / 2);
  screen.target().line(fui::Point{cx, static_cast<int16_t>(cy - r)}, fui::Point{cx, static_cast<int16_t>(cy + r)},
                       weight, paint);
  screen.target().line(fui::Point{static_cast<int16_t>(cx - dx), static_cast<int16_t>(cy - dy)},
                       fui::Point{static_cast<int16_t>(cx + dx), static_cast<int16_t>(cy + dy)}, weight, paint);
  screen.target().line(fui::Point{static_cast<int16_t>(cx - dx), static_cast<int16_t>(cy + dy)},
                       fui::Point{static_cast<int16_t>(cx + dx), static_cast<int16_t>(cy - dy)}, weight, paint);
}

// Two annotations that mean different things, and they now differ by GLYPH
// rather than by weight:
//
//   X          the PLAYER's own mark: "I have reasoned that this cell is
//              empty". Free, reversible, asserts nothing.
//   asterisk   the GAME's record: "you filled here and you were wrong".
//              Locked and counted.
//
// The mistake used to be a WHITE X on a SOLID BLACK cell, and that is what this
// change undoes. Mario, having played it: "The x is not heavy. What's heavy is
// the x with black background when a mistake is made." A solid cell is what the
// picture is MADE of, so every mistake added a black square to the emerging
// image and a board with a dozen of them showed a picture that was not the
// puzzle's. The mark has to stay visible and countable -- it does, it is the
// only six-armed thing on the board -- but it must not be content.
void drawCell(toybox::Screen& screen, const fui::Rect& cell, const picross::Cell state) {
  switch (state) {
    case picross::Cell::Filled:
      screen.target().fill(cell, fui::Paint::solid(fui::Color::Black));
      break;
    case picross::Cell::Crossed:
      drawX(screen, cell, fui::Color::Black);
      break;
    case picross::Cell::Mistake:
      // No fill behind it. That is the whole of the change.
      drawAsterisk(screen, cell, fui::Color::Black);
      break;
    case picross::Cell::Blank:
    default:
      break;
  }
}

// A symmetric "done" chip: a light dot screen centred on (cx, cy) and built
// from chip-local coordinates, so it is identical behind every satisfied clue
// and mirrors left-right and top-bottom about its own centre. Paint::dither
// cannot do this -- its 2x2 pattern is keyed to ABSOLUTE screen coordinates, so
// it lands on a different phase behind each clue and reads as lopsided, the
// defect Mario caught. 1px dots on a 2px lattice, anchored at the centre, are a
// ~25% grey that is mirror-symmetric by construction and phase-stable clue to
// clue.
void drawDoneChip(toybox::Screen& screen, const int16_t cx, const int16_t cy, const int16_t halfW,
                  const int16_t halfH) {
  const fui::Paint ink = fui::Paint::solid(fui::Color::Black);
  for (int16_t dy = 0; dy <= halfH; dy = static_cast<int16_t>(dy + 2)) {
    for (int16_t dx = 0; dx <= halfW; dx = static_cast<int16_t>(dx + 2)) {
      screen.target().fill(fui::makeRect(static_cast<int16_t>(cx + dx), static_cast<int16_t>(cy + dy), 1, 1), ink);
      if (dx)
        screen.target().fill(fui::makeRect(static_cast<int16_t>(cx - dx), static_cast<int16_t>(cy + dy), 1, 1), ink);
      if (dy)
        screen.target().fill(fui::makeRect(static_cast<int16_t>(cx + dx), static_cast<int16_t>(cy - dy), 1, 1), ink);
      if (dx && dy)
        screen.target().fill(fui::makeRect(static_cast<int16_t>(cx - dx), static_cast<int16_t>(cy - dy), 1, 1), ink);
    }
  }
}

// A clue number, centred in `box`. When its line is satisfied the number is
// crossed out -- honest here only because a wrong fill never becomes a Filled
// cell, so a satisfied count is a solved line and not a lucky miscount (see
// PicrossCore::rowSatisfied; the note there is load-bearing for the free-erase
// switch).
void drawClueNumber(toybox::Screen& screen, const fui::Rect& box, const int value, const ClueMetrics& cm,
                    const bool satisfied) {
  char text[toybox::kIntTextChars];
  std::snprintf(text, sizeof(text), "%d", value);
  fui::TextStyle style;
  style.font = cm.font;
  style.align = fui::TextAlign::Center;

  // inkCentred lands the digit's ink band centred in the box on both axes, so
  // the box centre IS the digit's centre.
  const int16_t cx = static_cast<int16_t>(box.x + box.width / 2);
  const int16_t cy = static_cast<int16_t>(box.y + box.height / 2);

  // A "0" clue is NEVER crossed out. An empty line has nothing to satisfy, so
  // the mark says nothing -- and a struck zero reads as a slashed zero or a
  // theta, which looks like a different symbol rather than a finished clue.
  const bool crossOut = satisfied && value > 0;

  // The cross-out is DIAGONAL, and it has to be. A horizontal rule through the
  // centre of a "1" IS a cross: the digit is a vertical stem, so a bar meeting
  // it at right angles makes a dagger no matter how far it overshoots or what
  // sits behind it -- geometry beats every cosmetic defence, and the earlier
  // centred-and-overshooting horizontal rule still read as a "t". A diagonal
  // still passes through the centre of the number (what Mario asked for) and
  // still reads unmistakably as "crossed out", but it cannot be mistaken for a
  // plus or a dagger because neither has a slanted bar. One consistent 45-degree
  // angle for EVERY struck clue -- a special case for the "1"s would itself look
  // like a bug. Chip first, then the digit, then the cross over both.
  int16_t chipHalfW = 0;
  int16_t chipHalfH = 0;
  if (crossOut) {
    const int16_t glyphW = screen.target().measureText(cm.font, text, style).width;
    chipHalfW = static_cast<int16_t>(glyphW / 2 + 4);
    chipHalfH = static_cast<int16_t>(cm.cut.inkHeight / 2 + 2);
    // Never spill into the neighbouring stacked clue or the playfield.
    if (chipHalfW > box.width / 2 - 1) chipHalfW = static_cast<int16_t>(box.width / 2 - 1);
    if (chipHalfH > box.height / 2 - 1) chipHalfH = static_cast<int16_t>(box.height / 2 - 1);
    if (chipHalfW > 0 && chipHalfH > 0) drawDoneChip(screen, cx, cy, chipHalfW, chipHalfH);
  }

  screen.target().text(toybox::inkCentred(screen.target(), box, style), text, style);

  if (crossOut && chipHalfW > 0 && chipHalfH > 0) {
    // Square the reach so the angle is the same on every clue, and keep it a
    // pixel inside the chip on both axes: the rule used to poke past the chip's
    // edges, which read as unfinished.
    int16_t reach = chipHalfW < chipHalfH ? chipHalfW : chipHalfH;
    reach = static_cast<int16_t>(reach - 1);
    if (reach > 0) {
      const uint8_t weight = static_cast<uint8_t>(cm.cut.inkHeight >= 20 ? 3 : 2);
      screen.target().line(fui::Point{static_cast<int16_t>(cx - reach), static_cast<int16_t>(cy + reach)},
                           fui::Point{static_cast<int16_t>(cx + reach), static_cast<int16_t>(cy - reach)}, weight,
                           fui::Paint::solid(fui::Color::Black));
    }
  }
}

void drawColClues(toybox::Screen& screen, const picross::Board& board, const Layout& layout, const ClueMetrics& cm) {
  uint8_t buf[picross::kMaxSize];
  for (int c = 0; c < board.size(); ++c) {
    const int k = board.colClues(c, buf);
    const bool satisfied = board.colSatisfied(c);
    const int16_t x = static_cast<int16_t>(layout.board.x + c * layout.cell);
    if (k == 0) {  // an all-empty column shows a single 0 nearest the board
      const fui::Rect box =
          fui::makeRect(x, static_cast<int16_t>(layout.board.y - kClueGap - cm.slotH), layout.cell, cm.slotH);
      drawClueNumber(screen, box, 0, cm, satisfied);
      continue;
    }
    for (int i = 0; i < k; ++i) {
      const int16_t top = static_cast<int16_t>(layout.board.y - kClueGap - (k - i) * cm.slotH);
      const fui::Rect box = fui::makeRect(x, top, layout.cell, cm.slotH);
      drawClueNumber(screen, box, buf[i], cm, satisfied);
    }
  }
}

void drawRowClues(toybox::Screen& screen, const picross::Board& board, const Layout& layout, const ClueMetrics& cm) {
  uint8_t buf[picross::kMaxSize];
  for (int r = 0; r < board.size(); ++r) {
    const int k = board.rowClues(r, buf);
    const bool satisfied = board.rowSatisfied(r);
    const int16_t y = static_cast<int16_t>(layout.board.y + r * layout.cell);
    if (k == 0) {
      const fui::Rect box =
          fui::makeRect(static_cast<int16_t>(layout.board.x - kClueGap - cm.numW), y, cm.numW, layout.cell);
      drawClueNumber(screen, box, 0, cm, satisfied);
      continue;
    }
    for (int i = 0; i < k; ++i) {
      const int16_t left = static_cast<int16_t>(layout.board.x - kClueGap - (k - i) * cm.numW);
      const fui::Rect box = fui::makeRect(left, y, cm.numW, layout.cell);
      drawClueNumber(screen, box, buf[i], cm, satisfied);
    }
  }
}

void drawGrid(toybox::Screen& screen, const picross::Board& board, const Layout& layout) {
  const int n = board.size();
  // White paper under a frame heavier than any line drawn inside it, so the
  // surface reads as one object. kBoardFrame is the weight Toybox reserves.
  screen.target().fill(layout.board, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(layout.board.inset(fui::Insets{-toybox::kBoardFrame, -toybox::kBoardFrame,
                                                        -toybox::kBoardFrame, -toybox::kBoardFrame}),
                         fui::Paint::solid(fui::Color::Black), toybox::kBoardFrame);
  for (int i = 1; i < n; ++i) {
    const int16_t x = static_cast<int16_t>(layout.board.x + i * layout.cell);
    const int16_t y = static_cast<int16_t>(layout.board.y + i * layout.cell);
    screen.target().fill(fui::makeRect(x, layout.board.y, toybox::kHairline, layout.board.height),
                         fui::Paint::solid(fui::Color::Black));
    screen.target().fill(fui::makeRect(layout.board.x, y, layout.board.width, toybox::kHairline),
                         fui::Paint::solid(fui::Color::Black));
  }
  // Major gridlines every five cells, the standard picross counting aid and the
  // one element the winning render borrowed from the others. Only visible on the
  // 10x10 boards; on a 5x5 the fives fall on the frame.
  for (int i = 5; i < n; i += 5) {
    const int16_t x = static_cast<int16_t>(layout.board.x + i * layout.cell - 1);
    const int16_t y = static_cast<int16_t>(layout.board.y + i * layout.cell - 1);
    screen.target().fill(fui::makeRect(x, layout.board.y, toybox::kRule, layout.board.height),
                         fui::Paint::solid(fui::Color::Black));
    screen.target().fill(fui::makeRect(layout.board.x, y, layout.board.width, toybox::kRule),
                         fui::Paint::solid(fui::Color::Black));
  }
  for (int r = 0; r < n; ++r)
    for (int c = 0; c < n; ++c) drawCell(screen, cellRect(layout, r, c), board.cell(r, c));
}

// Physical-button tool: FILL or MARK, also selected by the side keys.
// A short tap uses the selected tool; holding always marks an empty cell.
void drawModeToggle(toybox::Screen& screen, const fui::Rect& row, int mode) {
  for (int n = 0; n < 2; ++n) {
    const int gap = chrome::lightHeader ? 12 : 0;
    const int width = (row.width - gap) / 2;
    const auto r = fui::makeRect(row.x + n * (width + gap), row.y, width, row.height);
    const bool active = screen.frame().buttonSelected(ActionMode, n, mode == n);
    if (chrome::lightHeader)
      lightui::segment(screen.target(), r, 0, 1, active);
    else if (active)
      screen.target().fill(r, dashboard::ink);
    else
      dashboard::frame(screen, r, pixelui::kBorder);
    const auto paint = active ? dashboard::paper : dashboard::ink;
    const auto box = fui::makeRect(r.x + 12, r.y + (r.height - 24) / 2, 24, 24);
    if (n == 0)
      screen.target().fill(box, paint);
    else {
      screen.target().stroke(box, paint, 3);
      drawX(screen, box, active ? fui::Color::White : fui::Color::Black);
    }
    dashboard::text(screen, fui::makeRect(r.x + 44, r.y, r.width - 50, r.height),
                    n == 0 ? tr(STR_UI_FILL_57B284) : tr(STR_UI_MARK_20B7A6), row.width < 320 ? 3 : 0,
                    fui::TextAlign::Center, active);
    screen.frame().hit(r, ActionMode, n);
  }
}

// A plain action button. RESTART and PUZZLES sit a row above the mode toggle,
// away from the thumb, because RESTART discards the attempt.
void drawActionButton(toybox::Screen& screen, const fui::Rect& box, const char* label, int value) {
  gamepanel::row(screen, box, label, ActionButton, value,
                 value == ButtonRestart ? gamepanel::Mark::Restart : gamepanel::Mark::Grid, false, nullptr, false);
}

// The finished picture, drawn clean: black squares only, no lattice and no clue
// gutters. Used by both the win reveal (large) and a solved picker tile (tiny).
void drawPicture(toybox::Screen& screen, const picross::Puzzle& puzzle, const fui::Rect& area,
                 const fui::Color color = fui::Color::Black) {
  const int n = puzzle.size;
  const int16_t cell = static_cast<int16_t>((area.width < area.height ? area.width : area.height) / n);
  if (cell <= 0) return;
  const int16_t extent = static_cast<int16_t>(cell * n);
  const int16_t ox = static_cast<int16_t>(area.x + (area.width - extent) / 2);
  const int16_t oy = static_cast<int16_t>(area.y + (area.height - extent) / 2);
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      if ((puzzle.rows[r] & (uint16_t{1} << c)) == 0) continue;
      screen.target().fill(
          fui::makeRect(static_cast<int16_t>(ox + c * cell), static_cast<int16_t>(oy + r * cell), cell, cell),
          fui::Paint::solid(color));
    }
  }
}

}  // namespace

int stepPage(const int page, const int pageCount, const int delta) {
  if (pageCount <= 1) return 0;
  const int next = page + delta;
  if (next < 0) return 0;
  if (next >= pageCount) return pageCount - 1;
  return next;
}

bool Layout::cellAt(const int x, const int y, int& row, int& col) const {
  if (cell <= 0) return false;
  if (x < board.x || x >= board.right() || y < board.y || y >= board.bottom()) return false;
  col = (x - board.x) / cell;
  row = (y - board.y) / cell;
  return row >= 0 && row < size && col >= 0 && col < size;
}

int PickerLayout::indexAt(const int x, const int y) const {
  if (count <= 0) return -1;
  if (x < grid.x || y < grid.y || x >= grid.right() || y >= grid.bottom()) return -1;
  int local;
  if (rowHeight > 0) {  // a single-column list: one tile per row of `rowHeight`
    const int row = (y - grid.y) / rowHeight;
    if (row < 0) return -1;
    local = row;
  } else {  // a grid resolved by pitch, exactly as it was drawn
    if (cell <= 0) return -1;
    const int pitch = cell + gap;
    if ((x - grid.x) % pitch >= cell || (y - grid.y) % pitch >= cell) return -1;
    const int col = (x - grid.x) / pitch;
    const int row = (y - grid.y) / pitch;
    if (col < 0 || col >= cols || row < 0 || row >= rows) return -1;
    local = row * cols + col;
  }
  if (local < 0 || local >= count) return -1;
  return firstIndex + local;
}

void buildBoard(toybox::Screen& screen, const BoardModel& model, Layout& layout) {
  const picross::Board& board = *model.board;
  const int n = board.size();

  chrome(screen, "PICROSS", nullptr);
  screen.insetContent(fui::Insets{toybox::kGutter, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  const bool wide = screen.device().width > screen.device().height;
  fui::Rect hint, buttonRow, modeRow;
  if (!wide) {
    hint = screen.takeBottom(26, 6);
    buttonRow = screen.takeBottom(56, 12);
    modeRow = screen.takeBottom(60, 12);
  }
  const fui::Rect status = screen.takeTop(kStatusStrip, toybox::kGutter);
  auto body = screen.body();
  if (wide) {
    const int controlW = 288;
    const int x = body.right() - controlW, y = body.y + (body.height - 208) / 2;
    modeRow = fui::makeRect(x, y, controlW, 60);
    buttonRow = fui::makeRect(x, y + 80, controlW, 56);
    hint = fui::makeRect(x, y + 156, controlW, 52);
    body.width -= controlW + 16;
  }
  fittext::draw(screen.target(), hint, tr(STR_PICROSS_TOUCH_HINT), 3, wide ? 2 : 1, fui::TextAlign::Center);

  // The mistake count, the visible penalty. On the left, in plain words.
  char mistakes[kMistakeChars];
  if (board.mistakes() == 0)
    std::snprintf(mistakes, sizeof(mistakes), "%s", tr(STR_UI_NO_MISTAKES_D67257));
  else
    std::snprintf(mistakes, sizeof(mistakes), tr(STR_UI_MISTAKES_D_9071EC), board.mistakes());
  fui::TextStyle mistakeStyle;
  mistakeStyle.font = toybox::kUiFont;
  mistakeStyle.align = fui::TextAlign::Left;
  char title[64];
  const auto& puzzle = picross::kPuzzles[board.index()];
  std::snprintf(title, sizeof(title), tr(STR_PICROSS_MENU_PUZZLE), puzzle.size, puzzle.size,
                board.index() - groupFirst(groupFor(board.index())) + 1);
  dashboard::text(screen, fui::makeRect(status.x, status.y, status.width * 3 / 5, status.height), title, 0,
                  fui::TextAlign::Left);
  dashboard::text(screen, fui::makeRect(status.x + status.width * 3 / 5, status.y, status.width * 2 / 5, status.height),
                  mistakes, 3, fui::TextAlign::Right);

  // No size label here. There is one size, so "10 x 10" told the player nothing
  // they could act on and repeated itself on every screen; Mario's call. The
  // strip is the mistake count alone.

  const ClueMetrics cm = clueMetricsFor(n);
  const int16_t rowGutter = static_cast<int16_t>(maxRunsInRows(board) * cm.numW + kClueGap);
  const int16_t colGutter = static_cast<int16_t>(maxRunsInCols(board) * cm.slotH + kClueGap);

  int cell = (body.width - rowGutter) / n;
  const int availCellH = (body.height - colGutter) / n;
  if (availCellH < cell) cell = availCellH;
  if (cell > kMaxCell) cell = kMaxCell;
  if (cell < 1) cell = 1;

  const int boardExtent = cell * n;
  const int totalW = rowGutter + boardExtent;
  const int totalH = colGutter + boardExtent;
  const int16_t ox = static_cast<int16_t>(body.x + (body.width - totalW) / 2);
  const int16_t oy = static_cast<int16_t>(body.y + (body.height - totalH) / 2);

  layout.board = fui::makeRect(static_cast<int16_t>(ox + rowGutter), static_cast<int16_t>(oy + colGutter), boardExtent,
                               boardExtent);
  layout.cell = static_cast<int16_t>(cell);
  layout.size = static_cast<int16_t>(n);

  // One target for the whole grid, registered from the rect the cells were laid
  // out in and resolved back through the same Layout.
  screen.frame().hit(layout.board, ActionBoard, 0, fui::InputTouch | fui::InputPointerOnly);

  drawGrid(screen, board, layout);
  drawColClues(screen, board, layout, cm);
  drawRowClues(screen, board, layout, cm);

  drawModeToggle(screen, modeRow, model.mode);

  drawActionButton(screen, buttonRow, tr(STR_UI_RESTART_69ED00), ButtonRestart);
}

// Numbered picker grouped by puzzle size.
namespace {

// The bank is emitted size-sorted, so each size is one contiguous run and the
// groups are recoverable by scanning for the changes. Recovered once, for the
// tabs and for the paging.
//
// THE SLOTS ARE SIZED FROM picross::kSizeGroupCount, which the GENERATOR
// derives from the bank it just wrote. They were a literal 4 with a `break`
// underneath, and that pairing is a silent data-loss bug rather than a bound: a
// bank producing more groups than slots hits the break, and every puzzle after
// it is simply unreachable from the tabs -- nothing drawn wrong, nothing
// logged, nothing to see on the screen. FOUR IS EXACTLY THE BOUNDARY THAT BUG
// SAT ON, and this bank has four groups. Deriving the count means the array
// cannot be too small for a sorted bank; host-tests/picross asserts the
// sortedness AND that the recovered groups account for every puzzle in the
// bank, so the break below is unreachable rather than merely unlikely.
struct SizeGroups {
  int count = 0;
  int size[picross::kSizeGroupCount] = {};
  int start[picross::kSizeGroupCount] = {};
  int len[picross::kSizeGroupCount] = {};
};

SizeGroups sizeGroups() {
  SizeGroups g;
  for (int i = 0; i < picross::kPuzzleCount; ++i) {
    const int s = picross::kPuzzles[i].size;
    if (g.count == 0 || g.size[g.count - 1] != s) {
      if (g.count >= picross::kSizeGroupCount) break;
      g.size[g.count] = s;
      g.start[g.count] = i;
      g.len[g.count] = 0;
      ++g.count;
    }
    ++g.len[g.count - 1];
  }
  return g;
}

// Which group a puzzle index falls in. Used to open the picker on the tab
// holding the selection, so the activity never has to know where a run starts.
int groupOf(const SizeGroups& g, const int index) {
  for (int t = 0; t < g.count; ++t)
    if (index >= g.start[t] && index < g.start[t] + g.len[t]) return t;
  return 0;
}

// HOW MANY SIZE RUNS THE BANK ACTUALLY HAS, counted from the bank at compile
// time, and checked against the number the generator wrote down.
//
// The picker recovers its groups by run-scanning for changes of size, which is
// correct only while each size is ONE CONTIGUOUS RUN. An unsorted bank makes
// every alternation a new run: more runs than there are sizes, more groups than
// there are slots, the `break` in sizeGroups() fires, and every puzzle past that
// point is unreachable from the tabs. Nothing draws wrong. Nothing is logged.
//
// kSizeGroupCount is the number of DISTINCT sizes; this is the number of RUNS;
// they are equal if and only if the bank is size-sorted.
//
// BE PRECISE ABOUT WHAT THIS CATCHES: a HAND-EDITED header, or a bad merge of
// one. It does NOT catch an appended import, whatever a comment here used to
// claim -- gen_picross.emit() calls sort_by_size() unconditionally and derives
// kSizeGroupCount from the sorted result in the same pass, so a regenerate
// cannot produce a bank this assert would reject. The generator is the reason
// the property holds; this is the reason nobody can quietly edit it away
// afterwards. The other half of the guard is host-tests/picross, which catches
// the case this cannot: a hand-edit that bumps kSizeGroupCount to match its own
// damage still passes here and fails there, because the runs stop being
// ascending and stop accounting for every puzzle.
constexpr int countSizeRuns() {
  int runs = 0;
  for (int i = 0; i < picross::kPuzzleCount; ++i)
    if (i == 0 || picross::kPuzzles[i].size != picross::kPuzzles[i - 1].size) ++runs;
  return runs;
}

static_assert(picross::kSizeGroupCount >= 1,
              "a bank with no sizes has no puzzles; the generator refuses that long before here");
static_assert(countSizeRuns() == picross::kSizeGroupCount,
              "the bank is not size-sorted: it holds more runs of size than it holds distinct "
              "sizes. sizeGroups() run-scans for changes of size, so the extra runs overflow the "
              "group slots, the break fires, and every puzzle after that point is unreachable "
              "from the picker with nothing drawn wrong and nothing logged. Emit the bank through "
              "gen_picross.sort_by_size rather than appending to it.");

int clampInt(const int v, const int lo, const int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// A right-pointing "play" triangle: the resume affordance, drawn as narrowing
// horizontal bars so it needs no glyph and reads at any size.
void drawPlayGlyph(toybox::Screen& screen, const fui::Rect& box, const fui::Color color) {
  const int h = box.height;
  if (h <= 0 || box.width <= 0) return;
  const int centre = (h - 1) / 2;
  const int half = centre > 0 ? centre : 1;
  const fui::Paint ink = fui::Paint::solid(color);
  for (int i = 0; i < h; ++i) {
    const int dist = i <= centre ? centre - i : i - centre;
    const int w = box.width - (box.width * dist) / half;
    if (w <= 0) continue;
    screen.target().fill(fui::makeRect(box.x, static_cast<int16_t>(box.y + i), static_cast<int16_t>(w), 1), ink);
  }
}

// A row of page dots, the current one filled, each a tap target that jumps to
// its page. Silent when there is only one page.
void drawPageDots(toybox::Screen& screen, const fui::Rect& band, const int pageCount, const int current) {
  if (pageCount <= 1) return;
  pageui::build(screen, band, current + 1, pageCount, ActionPage, current - 1, ActionPage, current + 1);
}

// Completed tiles keep their number and reveal a small picture on black.
void drawTile(toybox::Screen& screen, const fui::Rect& box, const int index, const picross::Puzzle& puzzle,
              const bool solved, const bool inProgress, const bool selected) {
  const bool inverse = screen.frame().buttonSelected(ActionPick, index, selected || solved);
  const fui::Color bg = inverse ? fui::Color::Black : fui::Color::White;
  const fui::Color fg = inverse ? fui::Color::White : fui::Color::Black;
  lightui::surface(screen.target(), box, fui::Paint::solid(bg));
  lightui::outline(screen.target(), box, fui::Paint::solid(fui::Color::Black), 3);

  char number[toybox::kIntTextChars];
  std::snprintf(number, sizeof(number), "%d", index - groupFirst(groupFor(index)) + 1);
  numerals::draw(screen.target(), box.inset(fui::Insets{static_cast<int16_t>(solved ? 32 : 18), 12, 12, 12}), number,
                 fg);
  if (solved) {
    const auto corner = fui::makeRect(box.right() - 32, box.y + 5, 26, 26);
    drawPicture(screen, puzzle, corner, fg);
  }

  if (inProgress)
    drawPlayGlyph(screen, fui::makeRect(static_cast<int16_t>(box.x + 7), static_cast<int16_t>(box.y + 7), 9, 11), fg);
}

// How the tile grid fits `body`: four columns, and as many rows as the height
// actually holds.
//
// The rows used to be capped at four and the page at a literal sixteen, sized
// for a body that had a 60px size-tab band above it. With the tabs gone that
// literal would have left a band of dead paper under the last row and paged the
// bank more times than it needs -- a number derived once, then invalidated by a
// layout change that had no reason to know about it. Deriving it here means the
// page count follows the panel.
struct GridGeom {
  int16_t cols = 4;
  int16_t gap = 12;
  int16_t cell = 0;
  int16_t rows = 0;
  int16_t perPage = 0;
};

GridGeom gridGeom(const fui::Rect& body) {
  GridGeom g;
  g.cols = body.width > body.height ? 8 : 4;
  int cell = (body.width - (g.cols - 1) * g.gap) / g.cols;
  // A tile is a thumbnail of a 10x10 picture, not a poster: past this it is all
  // white space around a small drawing.
  if (cell > 118) cell = 118;
  if (cell < 1) cell = 1;
  g.cell = static_cast<int16_t>(cell);
  int rows = (body.height + g.gap) / (cell + g.gap);
  if (rows < 1) rows = 1;
  g.rows = static_cast<int16_t>(std::min(rows, body.width > body.height ? 2 : 4));
  g.perPage = static_cast<int16_t>(g.cols * g.rows);
  return g;
}

// Lay the tiles for puzzles [first, first+n) into `body`, filling `layout` so a
// tap resolves back through the same geometry.
void layOutGrid(toybox::Screen& screen, const MenuModel& model, const fui::Rect& body, const GridGeom& g,
                const int first, const int n, const int page, const int pageCount, PickerLayout& layout) {
  const int16_t rows = static_cast<int16_t>((n + g.cols - 1) / g.cols);
  const int16_t pitch = static_cast<int16_t>(g.cell + g.gap);
  const int16_t gridW = static_cast<int16_t>(g.cols * g.cell + (g.cols - 1) * g.gap);
  const int16_t gridH = static_cast<int16_t>(rows * g.cell + (rows - 1) * g.gap);
  const int16_t left = static_cast<int16_t>(body.x + (body.width - gridW) / 2);

  // Centred vertically in what the body actually has, and centred by the FULL
  // page rather than by this page's rows.
  //
  // The rows are derived from the height (gridGeom), so there is always a
  // remainder -- four rows of 103 leave 75px under the last one on this panel,
  // and top-aligned that reads as a grid the screen cut off rather than as
  // margin. It is genuinely a remainder: a fifth row needs 40px the tab band
  // took, and shrinking the tiles to buy it trades a legible thumbnail for a
  // row nobody asked for.
  //
  // BY THE FULL PAGE, because centring by `rows` would float a short last page
  // -- a final row of two tiles would sit halfway down the panel while every
  // other page starts under the tabs, and the grid would appear to jump as you
  // page. Every page's first row lands on the same y; a short page is simply
  // shorter at the bottom.
  const int16_t fullH = static_cast<int16_t>(g.rows * g.cell + (g.rows - 1) * g.gap);
  const int16_t slack = static_cast<int16_t>(body.height > fullH ? (body.height - fullH) / 2 : 0);
  const int16_t top = static_cast<int16_t>(body.y + slack);

  layout.grid = fui::makeRect(left, top, gridW, gridH);
  layout.cell = g.cell;
  layout.gap = g.gap;
  layout.cols = g.cols;
  layout.rows = rows;
  layout.count = static_cast<int16_t>(n);
  layout.firstIndex = static_cast<int16_t>(first);
  layout.pageCount = static_cast<int16_t>(pageCount);
  layout.pageOnScreen = static_cast<int16_t>(page);
  screen.frame().hit(layout.grid, ActionPick, -1, fui::InputTouch | fui::InputPointerOnly);

  for (int k = 0; k < n; ++k) {
    const int i = first + k;
    const int r = k / g.cols;
    const int c = k % g.cols;
    const fui::Rect box =
        fui::makeRect(static_cast<int16_t>(left + c * pitch), static_cast<int16_t>(top + r * pitch), g.cell, g.cell);
    screen.frame().hit(box, ActionPick, i);
    const bool solved = model.progress != nullptr && model.progress->isSolved(i);
    drawTile(screen, box, i, picross::kPuzzles[i], solved, i == model.inProgressIndex, i == model.selectedIndex);
  }
}

}  // namespace

void buildPicker(toybox::Screen& screen, const MenuModel& model, PickerLayout& layout) {
  layout = PickerLayout{};
  chrome(screen, "PICROSS", nullptr);
  screen.insetContent(fui::Insets{toybox::kGutter, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  const SizeGroups groups = sizeGroups();
  const auto caption = screen.takeTop(34, 6);
  dashboard::text(screen, caption, tr(STR_PICROSS_SELECT), 0, fui::TextAlign::Left);
  const auto selectedBand = screen.takeBottom(32, 8);
  const fui::Rect dotBand = screen.takeBottom(44, 8);

  const GridGeom g = gridGeom(screen.body());
  const int perPage = g.perPage;

  // followSelection is how the picker OPENS on the puzzle PLAY would start. It
  // resolves BOTH the tab and the page, and that is the point: the activity
  // knows neither how many tiles fit a page nor where one size run ends, and a
  // second copy of either is a copy that goes wrong the next time the layout or
  // the bank moves. That is not hypothetical -- the page was once computed in
  // the activity by dividing by a literal 16, and it went silently wrong the
  // moment this band's height changed.
  const int selected = clampInt(model.selectedIndex, 0, picross::kPuzzleCount - 1);
  // Clamped to 0 and not to groups.count - 1 when there are no groups: on an
  // empty bank that upper bound is -1, and clampInt would hand back -1, which
  // then indexes groups.start[] below. Two of the three uses of `tab` were
  // already defended against that and the third was not -- the shape this fork
  // calls "bounding one of two input paths". kPuzzleCount is never zero (the
  // generator refuses an empty bank and a static_assert backs it), so this is
  // insurance rather than a live bug, but a bound that only holds because of a
  // fact two files away is not a bound.
  const int lastTab = groups.count > 0 ? groups.count - 1 : 0;
  const int tab = model.followSelection ? groupOf(groups, selected) : clampInt(model.sizeTab, 0, lastTab);

  const int len = groups.count > 0 ? groups.len[tab] : 0;
  const int pageCount = len > 0 ? (len + perPage - 1) / perPage : 1;
  const int page = model.followSelection ? clampInt((selected - groups.start[tab]) / perPage, 0, pageCount - 1)
                                         : clampInt(model.page, 0, pageCount - 1);

  layout.tabOnScreen = tab;
  layout.tabCount = groups.count;
  char sizeLabel[20];
  std::snprintf(sizeLabel, sizeof(sizeLabel), "%dx%d", groups.size[tab], groups.size[tab]);
  dashboard::text(screen, fui::makeRect(caption.right() - 80, caption.y, 80, caption.height), sizeLabel, 0,
                  fui::TextAlign::Right);
  char selectedLabel[80];
  const char* state = model.progress && model.progress->isSolved(selected) ? tr(STR_PICROSS_DONE)
                      : selected == model.inProgressIndex                  ? tr(STR_PICROSS_IN_PROGRESS)
                                                                           : tr(STR_PICROSS_NOT_STARTED);
  std::snprintf(selectedLabel, sizeof(selectedLabel), tr(STR_PICROSS_PICK_STATUS),
                selected - groups.start[groupOf(groups, selected)] + 1, state);
  dashboard::text(screen, selectedBand, selectedLabel, 0);

  const int firstInGroup = page * perPage;
  const int first = (groups.count > 0 ? groups.start[tab] : 0) + firstInGroup;
  const int n = len - firstInGroup < perPage ? len - firstInGroup : perPage;
  layOutGrid(screen, model, screen.body(), g, first, n, page, pageCount, layout);
  drawPageDots(screen, dotBand, pageCount, page);
}

int groupFor(int index) { return groupOf(sizeGroups(), std::clamp(index, 0, picross::kPuzzleCount - 1)); }
int groupFirst(int group) {
  const auto groups = sizeGroups();
  return groups.start[std::clamp(group, 0, groups.count - 1)];
}
namespace {
void example(toybox::Screen& screen, fui::Rect r) {
  static constexpr uint8_t lines[] = {4, 14, 31, 14, 4};
  static constexpr int clues[] = {1, 3, 5, 3, 1};
  const int cell = std::min({64, int(r.width) / 6, int(r.height) / 6});
  if (cell < 1) return;
  const int x = r.x + (r.width - cell * 6) / 2 + cell, y = r.y + (r.height - cell * 6) / 2 + cell;
  for (int row = 0; row < 5; ++row) {
    char label[4];
    std::snprintf(label, sizeof(label), "%d", clues[row]);
    dashboard::text(screen, fui::makeRect(x - cell, y + row * cell, cell, cell), label, cell < 28 ? 3 : 0);
    dashboard::text(screen, fui::makeRect(x + row * cell, y - cell, cell, cell), label, cell < 28 ? 3 : 0);
    for (int col = 0; col < 5; ++col) {
      const auto box = fui::makeRect(x + col * cell, y + row * cell, cell, cell);
      dashboard::frame(screen, box, 2);
      if (lines[row] & (1 << col)) screen.target().fill(box.inset(fui::Insets{3, 3, 3, 3}), dashboard::ink);
    }
  }
}
void lessonRow(toybox::Screen& screen, fui::Rect r, bool gaps) {
  const int cell = std::min(int(r.height), int(r.width) / 7), width = cell * 6;
  const int x = r.x + (r.width - width) / 2, y = r.y + (r.height - cell) / 2;
  dashboard::text(screen, fui::makeRect(x, y, cell, cell), gaps ? "1 1" : "3", cell < 30 ? 3 : 0);
  for (int n = 0; n < 5; ++n) {
    const auto box = fui::makeRect(x + (n + 1) * cell, y, cell, cell);
    dashboard::frame(screen, box, 2);
    if (gaps ? (n == 0 || n == 2) : (n >= 1 && n <= 3))
      screen.target().fill(box.inset(fui::Insets{3, 3, 3, 3}), dashboard::ink);
    else if (gaps)
      drawX(screen, box, fui::Color::Black);
  }
}
}  // namespace
void buildMenu(toybox::Screen& screen, const MenuModel& model, PickerLayout& layout) {
  layout = {};
  frontdoor::begin(screen, "PICROSS");
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  const auto left = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 10, body.height) : body;
  const auto right =
      wide ? fui::makeRect(left.right() + 20, body.y, body.right() - left.right() - 20, body.height) : body;
  const int rowH = wide ? 60 : 62, top = body.bottom() - 4 * rowH - 30;
  char label[80];
  std::snprintf(label, sizeof(label), tr(STR_GUIDE_PICROSS_SOLVED), model.solvedCount, model.total);
  const auto tally = fui::makeRect(left.x + left.width / 10, left.y + 4, left.width * 8 / 10, 50);
  dashboard::frame(screen, tally, pixelui::kBorder);
  dashboard::icon(screen, fui::makeRect(tally.x + 12, tally.y + 10, 30, 30), extra_check_32);
  dashboard::text(screen, fui::makeRect(tally.x + 52, tally.y, tally.width - 62, tally.height), label, 0);
  const auto& puzzle = picross::kPuzzles[std::clamp(model.selectedIndex, 0, picross::kPuzzleCount - 1)];
  const int end = wide ? left.bottom() - 40 : top - 42;
  example(screen, fui::makeRect(left.x, left.y + 62, left.width, end - left.y - 62));
  std::snprintf(label, sizeof(label), tr(STR_PICROSS_MENU_PUZZLE), puzzle.size, puzzle.size,
                model.selectedIndex - groupFirst(groupFor(model.selectedIndex)) + 1);
  dashboard::text(screen, fui::makeRect(left.x, end, left.width, 34), label, 0);
  char size[16];
  std::snprintf(size, sizeof(size), "%dx%d", puzzle.size, puzzle.size);
  const char* labels[] = {tr(STR_WORDLE_PLAY), tr(STR_PICROSS_SELECT), tr(STR_PICROSS_SIZE), tr(STR_PICROSS_HOW)};
  const int values[] = {ButtonPlay, ButtonPuzzles, ButtonSizes, ButtonHelp};
  const gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::Grid, gamepanel::Mark::Grid,
                                   gamepanel::Mark::Help};
  const int start = wide ? body.y + (body.height - 4 * rowH - 30) / 2 : top;
  for (int n = 0; n < 4; ++n)
    gamepanel::row(screen, fui::makeRect(right.x, start + n * (rowH + 10), right.width, rowH), labels[n], ActionButton,
                   values[n], marks[n], n == 0, n == 2 ? size : nullptr);
}
void buildSizes(toybox::Screen& screen, const MenuModel& model) {
  frontdoor::begin(screen, "PICROSS");
  dashboard::text(screen, screen.takeTop(48, 12), tr(STR_PICROSS_SIZE), 1, fui::TextAlign::Left);
  dashboard::button(screen, screen.takeBottom(62, 0), tr(STR_GUIDE_APPLY), ActionApplySize, 0, true, &extra_check_32,
                    pixelui::kBody);
  const auto groups = sizeGroups();
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  const int gap = 12, w = wide ? (body.width - gap) / 2 : body.width,
            rows = wide ? (groups.count + 1) / 2 : groups.count,
            h = std::min(110, (int(body.height) - (rows - 1) * gap) / rows);
  for (int n = 0; n < groups.count; ++n) {
    const bool active = n == model.sizeTab;
    const auto box = fui::makeRect(body.x + (wide ? n % 2 * (w + gap) : 0),
                                   body.y + (wide ? n / 2 * (h + gap) : n * (h + gap)), w, h);
    if (active)
      lightui::surface(screen.target(), box, dashboard::ink);
    else
      dashboard::frame(screen, box, pixelui::kBorder);
    guideui::grid(screen, fui::makeRect(box.x + 16, box.y + 14, 72, h - 28), groups.size[n], active, true);
    char label[40];
    std::snprintf(label, sizeof(label), "%d x %d", groups.size[n], groups.size[n]);
    dashboard::text(screen, fui::makeRect(box.x + 104, box.y + 12, box.width - 164, 40), label, 1, fui::TextAlign::Left,
                    active);
    std::snprintf(label, sizeof(label), tr(STR_GUIDE_CELLS), groups.size[n] * groups.size[n]);
    dashboard::text(screen, fui::makeRect(box.x + 104, box.y + 58, box.width - 164, 30), label, 3, fui::TextAlign::Left,
                    active);
    const auto paint = active ? dashboard::paper : dashboard::ink;
    const auto check = fui::makeRect(box.right() - 46, box.y + (h - 30) / 2, 30, 30);
    if (active)
      screen.target().fill(check, paint);
    else
      screen.target().stroke(check, paint, 3);
    screen.frame().hit(box, ActionTab, n);
  }
}
void buildHelp(toybox::Screen& screen, int page) {
  page = std::clamp(page, 0, 3);
  frontdoor::begin(screen, "PICROSS");
  dashboard::text(screen, screen.takeTop(screen.body().width > screen.body().height ? 34 : 48, 8), tr(STR_PICROSS_HOW),
                  1, fui::TextAlign::Left);
  pageui::build(screen, screen.takeBottom(48, 12), page + 1, 4, ActionHelpPage, -1, ActionHelpPage, 1);
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  dashboard::frame(screen, body, pixelui::kBorder);
  const int pad = 14;
  guideui::badge(screen, fui::makeRect(body.x + pad, body.y + pad, 38, 38), page + 1);
  static constexpr StrId titles[] = {StrId::STR_PICROSS_READ_CLUES, StrId::STR_PICROSS_LEAVE_GAP,
                                     StrId::STR_GUIDE_PICROSS_MARK, StrId::STR_GUIDE_PICROSS_FINISH};
  dashboard::text(screen, fui::makeRect(body.x + 62, body.y + pad, body.width - 76, 38), I18N.get(titles[page]), 1,
                  fui::TextAlign::Left);
  const auto content = fui::makeRect(body.x + pad, body.y + 64, body.width - 2 * pad, body.height - 78);
  if (page < 2) {
    const auto copy = wide ? fui::makeRect(content.x, content.y, content.width / 2 - 12, content.height - 50)
                           : fui::makeRect(content.x, content.y, content.width, 112);
    guideui::text(screen, copy, page == 0 ? tr(STR_GUIDE_PICROSS_CLUES) : tr(STR_PICROSS_GAP_BODY), 4);
    const auto demo = wide ? fui::makeRect(content.x + content.width / 2, content.y, content.width / 2, 68)
                           : fui::makeRect(content.x, content.y + 130, content.width, 96);
    lessonRow(screen, demo, page == 1);
    dashboard::text(screen, fui::makeRect(demo.x, demo.bottom() + 10, demo.width, wide ? 28 : 36),
                    page == 0 ? tr(STR_GUIDE_PICROSS_THREE) : tr(STR_GUIDE_PICROSS_TWO), 0);
    const auto notice = fui::makeRect(content.x, content.bottom() - (wide ? 36 : 64), content.width, wide ? 36 : 64);
    dashboard::frame(screen, notice, pixelui::kBorder);
    dashboard::text(screen, notice.inset(fui::Insets{8, 8, 8, 8}),
                    page == 0 ? tr(STR_GUIDE_PICROSS_ORDER) : tr(STR_GUIDE_PICROSS_X), 0);
  } else if (page == 2) {
    static constexpr StrId names[] = {StrId::STR_PICROSS_FILL, StrId::STR_UI_MARK_20B7A6, StrId::STR_PICROSS_SELECTED};
    static constexpr StrId copy[] = {StrId::STR_GUIDE_PICROSS_FILL_BODY, StrId::STR_GUIDE_PICROSS_EMPTY_BODY,
                                     StrId::STR_GUIDE_PICROSS_SELECTION_BODY};
    const int h = wide ? content.height - 34 : (content.height - 42) / 3, w = wide ? content.width / 3 : content.width;
    for (int n = 0; n < 3; ++n) {
      const auto row = fui::makeRect(content.x + (wide ? n * w : 0), content.y + (wide ? 0 : n * h), w, h);
      const int side = std::max(24, std::min(64, wide ? 56 : h - 32));
      const int artX = wide ? row.x + (row.width - side) / 2 : row.x + 30;
      const auto box = fui::makeRect(artX, row.y + 4, side, side);
      dashboard::frame(screen, box, pixelui::kBorder);
      if (n == 0)
        screen.target().fill(box.inset(fui::Insets{7, 7, 7, 7}), dashboard::ink);
      else if (n == 1)
        drawX(screen, box, fui::Color::Black);
      else
        dashboard::frame(screen, box.inset(fui::Insets{6, 6, 6, 6}), pixelui::kBorder);
      dashboard::text(screen, fui::makeRect(wide ? row.x : row.x, row.y + side + 8, wide ? row.width : 120, 24),
                      I18N.get(names[n]), 3);
      const auto desc = wide ? fui::makeRect(row.x + 8, row.y + side + 36, row.width - 16, row.height - side - 36)
                             : fui::makeRect(row.x + 140, row.y + 12, row.width - 150, h - 20);
      guideui::text(screen, desc, I18N.get(copy[n]), 3, wide ? fui::TextAlign::Center : fui::TextAlign::Left, 4);
    }
    dashboard::text(screen, fui::makeRect(content.x, content.bottom() - 32, content.width, 32),
                    tr(STR_PICROSS_TOUCH_HINT), 3);
  } else {
    const auto copy = wide ? fui::makeRect(content.x, content.y, content.width / 2 - 12, content.height - 40)
                           : fui::makeRect(content.x, content.y, content.width, 112);
    guideui::text(screen, copy, tr(STR_GUIDE_PICROSS_FINISH_BODY), 4);
    const auto art =
        wide ? fui::makeRect(content.x + content.width / 2, content.y, content.width / 2, content.height - 40)
             : fui::makeRect(content.x, content.y + 120, content.width, content.height - 162);
    example(screen, art);
    dashboard::text(screen, fui::makeRect(content.x, content.bottom() - 34, content.width, 34),
                    tr(STR_GUIDE_PICROSS_ALL), 3);
  }
}

void buildWin(toybox::Screen& screen, const WinModel& model) {
  chrome(screen, tr(STR_UI_SOLVED_39E6BE), nullptr);
  screen.insetContent(fui::Insets{toybox::kGutter, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  const fui::Rect actions = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);
  const int16_t half = static_cast<int16_t>((actions.width - toybox::kGutter) / 2);
  {
    fui::ButtonProps next;
    next.label = model.moreToPlay ? tr(STR_UI_NEXT_1992D5) : tr(STR_UI_PUZZLES_915460);
    next.action = ActionButton;
    next.value = model.moreToPlay ? ButtonNext : ButtonPuzzles;
    next.text = toybox::buttonText(screen.theme());
    next.radius = 0;
    screen.button(next, fui::makeRect(actions.x, actions.y, half, actions.height));

    fui::ButtonProps back;
    back.label = tr(STR_UI_PUZZLES_915460);
    back.action = ActionButton;
    back.value = ButtonPuzzles;
    back.text = toybox::buttonText(screen.theme());
    back.radius = 0;
    back.styles = toybox::buttonStyles();
    screen.button(
        back, fui::makeRect(static_cast<int16_t>(actions.x + half + toybox::kGutter), actions.y, half, actions.height));
  }

  // The grade, at the foot: the reveal's punchline, and the reason to replay.
  const fui::Rect gradeBand = screen.takeBottom(40, toybox::kMargin);
  char grade[kGradeChars];
  if (model.mistakes == 0)
    std::snprintf(grade, sizeof(grade), "%s", tr(STR_UI_PERFECT_NO_MISTAKES_96B521));
  else if (model.mistakes == 1)
    std::snprintf(grade, sizeof(grade), "%s", tr(STR_UI_SOLVED_WITH_1_MISTAKE_9B66E2));
  else
    std::snprintf(grade, sizeof(grade), tr(STR_UI_SOLVED_WITH_D_MISTAKES_3DE241), model.mistakes);
  fui::TextStyle gradeStyle;
  gradeStyle.font = toybox::kUiFont;
  gradeStyle.align = fui::TextAlign::Center;
  screen.target().text(toybox::inkCentred(screen.target(), gradeBand, gradeStyle), grade, gradeStyle);

  // NO DESIGNER CREDIT HERE, AND THERE IS NOBODY TO CREDIT.
  //
  // It used to read "PUZZLE BY <name>" under the reveal, for a bank of six named
  // designers' work used by permission. Mario, having seen it: "it just looks
  // bad" -- and then, on the flash it cost, "as long as it doesn't reach
  // firmware anywhere and uses space there I'm good". Those puzzles are gone.
  // The current bank carries no attribution obligation at all: no author, no
  // licence, no source, and no file anywhere recording one. See the note at the
  // top of PicrossPuzzles.h, which is the whole of what is claimed.
  //
  // This comment used to say the obligation was met in
  // assets_local/picross/PROVENANCE.md. That file was deleted with the puzzles
  // it credited, and this line outlived it -- the last place in the firmware
  // still asserting these pictures have designers on record.

  // The revealed name, now safe to show -- and the WHOLE reward, which is why
  // the picker and the board hide it.
  //
  // An UNNAMED puzzle draws no band at all rather than an empty one: a name is
  // the puzzle's title in the source file and a corpus may not carry one, so a bank is normally part-named, and a blank
  // 52px gap over every unnamed picture would read as a name that failed to
  // render (see the a-silent-screen-reads-as-a-crash memory). No band means the
  // picture simply gets the space.
  const bool named = model.cleared != nullptr && model.cleared->name != nullptr && model.cleared->name[0] != '\0';
  if (named) {
    const fui::Rect nameBand = screen.takeBottom(52, toybox::kGutter);
    fui::TextStyle nameStyle;
    nameStyle.font = toybox::kDisplayFont;
    nameStyle.align = fui::TextAlign::Center;
    const std::string fitted = toybox::fittedTitle(screen.target(), model.cleared->name, nameBand.width, nameStyle);
    screen.target().text(toybox::inkCentred(screen.target(), nameBand, nameStyle), fitted.c_str(), nameStyle);
  }

  // The picture itself, clean, filling what is left -- the payoff.
  if (model.cleared != nullptr) drawPicture(screen, *model.cleared, screen.body());
}

}  // namespace picrossui
