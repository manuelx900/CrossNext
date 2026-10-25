#include <FreeInkUICore.h>

#include <cstdio>

namespace ui = freeink::ui;
static int checks = 0, failures = 0;
static void check(bool value, const char* message) {
  ++checks;
  if (!value) {
    ++failures;
    std::printf("FAIL: %s\n", message);
  }
}
int main() {
  ui::InteractionBuffer<5> controls;
  controls.addInteraction({{10, 100, 200, 50}, 1, 11, ui::InputTouch});
  controls.addInteraction({{10, 160, 200, 50}, 1, 12, ui::InputTouch, ui::StateDisabled});
  controls.addInteraction({{10, 220, 200, 50}, 1, 13, ui::InputTouch});
  controls.addInteraction({{10, 300, 50, 40}, 2, -1, ui::InputTouch | ui::InputPrev});
  controls.addInteraction({{160, 300, 50, 40}, 2, 1, ui::InputTouch | ui::InputNext});
  ui::Rect rect;
  check(!controls.hasButtonFocus() && !controls.buttonFocusRect(rect), "Touch-only screen starts without cursor");
  ui::InputSnapshot input{};
  input.confirm = true;
  check(!controls.route(input), "Confirm cannot activate an unselected item");
  input = {};
  input.focusNext = true;
  check(!controls.route(input) && controls.focusedIndex() == 0, "First press selects first row without activating it");
  check(controls.buttonFocusRect(rect) && rect.x == 10 && rect.y == 100, "Focused geometry is exact");
  input = {};
  input.confirm = true;
  auto event = controls.route(input);
  check(event.action == 1 && event.value == 11, "Focused touch row can be confirmed");
  input = {};
  input.focusNext = true;
  controls.route(input);
  check(controls.focusedIndex() == 2, "Disabled rows are skipped");
  input = {};
  input.focusPrev = true;
  controls.route(input);
  check(controls.focusedIndex() == 0, "Previous row skips disabled row");
  input = {};
  input.next = true;
  event = controls.route(input);
  check(event.action == 2 && event.value == 1, "Held next uses the visible pager's action");
  input = {};
  input.prev = true;
  event = controls.route(input);
  check(event.action == 2 && event.value == -1, "Held previous uses the visible pager's action");
  input = {};
  input.touchReleased = true;
  input.touchX = 15;
  input.touchY = 225;
  event = controls.route(input);
  check(event.value == 13 && !controls.hasButtonFocus(), "Touch activation restores cursor-free appearance");
  for (int n = 0; n < 100; ++n) {
    input = {};
    input.focusNext = true;
    controls.route(input);
    check(controls.focusedIndex() != 1, "Repeated navigation never selects disabled entry");
  }
  controls.clear();
  input = {};
  input.confirm = true;
  check(!controls.route(input), "Stale focus cannot activate after menu change");
  input = {};
  input.next = true;
  check(!controls.route(input), "An absent pager cannot invent a next action");
  ui::InteractionBuffer<2> board;
  board.addInteraction({{20, 40, 90, 90}, 7, -1, ui::InputTouch});
  board.addInteraction({{20, 40, 90, 90},
                        ui::NO_ACTION,
                        0,
                        ui::InputFocus | ui::InputConfirm | ui::InputCoordinates,
                        ui::StateNormal,
                        0,
                        3,
                        3});
  board.setFocusedIndex(1);
  check(!board.buttonFocusRect(rect), "Programmatic selection does not show button cursor");
  for (int cell = 0; cell < 9; ++cell) {
    input = {};
    input.focusNext = true;
    // Enter the coordinate grid from the preceding regular control.
    if (!cell) board.setFocusedIndex(0);
    const auto previous = board.focusPosition();
    board.route(input);
    check(previous != board.focusPosition(), "Every cell movement has a distinct repaint position");
    check(board.buttonFocusRect(rect) && rect.x == 20 + (cell % 3) * 30 && rect.y == 40 + (cell / 3) * 30,
          "Grid focus maps to cell centre in drawn coordinates");
    input = {};
    input.confirm = true;
    event = board.route(input);
    check(event.action == 7 && event.value == -1, "Coordinate confirmation reuses original touch action");
    // The flag intentionally remains on across confirmations and cell changes.
    if (cell < 8) {
      input = {};
      input.touchReleased = true;
      input.touchX = -1;
      input.touchY = -1;
      board.route(input);
    }
  }
  std::printf("Side navigation: %d checks, %d failures; buffer bytes=%zu\n", checks, failures, sizeof(controls));
  return failures ? 1 : 0;
}
