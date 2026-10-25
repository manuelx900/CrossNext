#pragma once

#include "../../ui/ToyboxScreen.h"
#include "../PieceDesign.h"
#include "ChessPuzzleCore.h"

namespace chesspuzzleui {
enum : freeink::ui::ActionId {
  Difficulty = 1,
  Restart,
  Previous,
  Next,
  Choose,
  Reset,
  ConfirmReset,
  Cancel,
  Resume,
  Promote,
  Review,
  Page,
  Pick
};
enum class View : uint8_t { Difficulty, Select, Board, ConfirmReset, Promotion, Celebration };
struct Model {
  const chesspuzzle::Session* session = nullptr;
  const chesspuzzle::Progress* progress = nullptr;
  View view = View::Difficulty;
  int group = 0, index = 0, page = 0;
  bool saveFailed = false;
  bool invalidNumber = false;
  chessstyle::Design pieceDesign = chessstyle::kDefault;
};
struct Work {
  freeink::ui::ButtonProps button;
  char labels[5][128]{};
  char buttonLabel[128]{};
};
freeink::ui::Rect boardRect(const freeink::ui::DeviceContext& device);
int squareAt(const freeink::ui::DeviceContext& device, bool white, int x, int y);
void build(toybox::Screen& screen, const Model& model, Work& work);
}  // namespace chesspuzzleui
