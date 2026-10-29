#include <cstdio>

#include "../../src/apps_local/picross/PicrossScreens.h"

int main() {
  int checks = 0, failures = 0;
  auto check = [&](bool value) {
    ++checks;
    if (!value) ++failures;
  };
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
