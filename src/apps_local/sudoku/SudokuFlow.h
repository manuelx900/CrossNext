#pragma once

// Sudoku's navigation, and nothing else. Freestanding.
//
// Navigation screens and no game machine, for the reason Minesweeper's flow gives:
// this is a solitaire, so "what state is the game in" already has one home, in
// the rules, and restating it here would be two facts that have to agree.
//
// What this file does own is the one thing that is genuinely about the shell:
// where Back goes.

#include <cstdint>

namespace sudoku {

enum class Screen : uint8_t {
  // The top. Back from here leaves the app, and it is the only screen that does.
  Menu,
  HowTo,
  Settings,
  Board,
  Result,
  ConfirmNew,
};

// Where Back goes from `screen`. Exhaustive switch and no default, so a new
// screen without a decided Back fails the build rather than falling through to
// something plausible.
constexpr Screen back(const Screen screen) {
  switch (screen) {
    case Screen::Menu:
      // The caller leaves the app; returning Menu keeps this total without
      // inventing a state for "gone".
      return Screen::Menu;
    case Screen::ConfirmNew:
    case Screen::HowTo:
    case Screen::Settings:
      return Screen::Menu;
    case Screen::Board:
      // Leaving a puzzle goes to the menu, not out of the app: the first Back
      // means stop solving, the second means leave. The puzzle is saved either
      // way, so this is never destructive.
      return Screen::Menu;
    case Screen::Result:
      // Back from the stats returns to the finished grid, which is the thing
      // worth looking at. Result is a door off the board, not a place the game
      // sends you.
      return Screen::Board;
  }
  return Screen::Menu;
}

constexpr bool leavesApp(const Screen screen) { return screen == Screen::Menu; }

// Completion opens Result once. Returning to the solved Board keeps it read-only.

}  // namespace sudoku
