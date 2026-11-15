#include <cstdio>

#include "../../src/apps_local/picross/PicrossPaintState.h"
#include "../../src/apps_local/picross/PicrossScreens.h"

int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool value) {
    ++checks;
    if (!value) ++failures;
  };
  uint8_t pixels[128]{};
  picross::PaintState paint;
  const auto originalPixels = picross::PaintState::fingerprint(pixels, sizeof(pixels));
  check(!paint.unchanged(originalPixels, 1, 7, 0));
  paint.remember(originalPixels, 1, 7, 0);
  for (unsigned n = 0; n < sizeof(pixels); ++n) {
    check(paint.unchanged(originalPixels, 1, 7, 0));
    pixels[n] = 1;
    check(!paint.unchanged(picross::PaintState::fingerprint(pixels, sizeof(pixels)), 1, 7, 0));
    pixels[n] = 0;
  }
  check(!paint.unchanged(originalPixels, 2, 7, 0));  // A panel was opened over the game.
  check(!paint.unchanged(originalPixels, 1, 8, 0));  // Same pixels, different hit meaning.
  check(!paint.unchanged(originalPixels, 1, 7, 1));  // Rotation or polarity changed.
  paint.invalidate();
  check(!paint.unchanged(originalPixels, 1, 7, 0));
  // A grid-only update does not show a newly rebuilt header. The later full
  // repaint must remain eligible even when its framebuffer is identical.
  paint.remember(originalPixels, 3, 7, 0);
  pixels[0] = 1;
  pixels[64] = 1;
  const auto changedHeader = picross::PaintState::fingerprint(pixels, sizeof(pixels));
  paint.invalidate();
  check(!paint.unchanged(changedHeader, 4, 7, 0));
  picross::Board original;
  original.load(0);
  for (int selected : {picrossui::ModeFill, picrossui::ModeMark}) {
    for (bool held : {false, true}) {
      for (int row = 0; row < original.size(); ++row) {
        for (int col = 0; col < original.size(); ++col) {
          auto board = original;
          const int tool = picrossui::tapMode(selected, held);
          if (selected == picrossui::ModeMark || held) {
            check(tool == picrossui::ModeMark);
            check(board.mark(row, col));
            check(board.cell(row, col) == picross::Cell::Crossed);
            check(board.mistakes() == 0);
            check(board.mark(row, col) && board.cell(row, col) == picross::Cell::Blank);
          } else {
            check(tool == picrossui::ModeFill);
            check(board.fill(row, col));
            check(board.cell(row, col) == (original.solid(row, col) ? picross::Cell::Filled : picross::Cell::Mistake));
          }
        }
      }
    }
  }
  std::printf("Picross tool input: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
