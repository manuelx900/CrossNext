#pragma once
#include "../ui/ToyboxScreen.h"
#include "Game2048Core.h"
#include "Game2048Result.h"
namespace game2048ui {
enum : freeink::ui::ActionId { Play = 1, New, Help, Move, ConfirmNew, CancelNew, Review };
enum class View { Menu, Board, Help, Confirm, Result, Review };
struct Model {
  const game2048::State* state = nullptr;
  uint64_t best = 0;
  View view = View::Menu;
  bool saveFailed = false;
  int selected = 0;
  game2048::Result result = game2048::Result::None;
};
freeink::ui::Rect boardRect(const freeink::ui::DeviceContext& device);
struct Work {
  freeink::ui::ButtonProps button;
  char buttonLabel[96]{};
};
void build(toybox::Screen& screen, const Model& model, Work& work);
}  // namespace game2048ui
