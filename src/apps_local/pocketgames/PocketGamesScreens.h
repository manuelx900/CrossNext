#pragma once
#include "../ui/ToyboxScreen.h"
#include "PocketGamesCore.h"

namespace pocketui {
namespace fui = freeink::ui;
enum class View : uint8_t { Menu, Board, Help, ConfirmNew, Level, Result, Review };
enum : fui::ActionId {
  Play = 1,
  New,
  Help,
  ConfirmNew,
  Cancel,
  Restart,
  Menu,
  GuidePage,
  ViewBoard,
  ViewResult,
  Settings
};
struct Model {
  const pocketgames::State* state = nullptr;
  View view = View::Menu;
  bool saveFailed = false, invalid = false;
  int selected = -1;
  int level = 0;
  int guidePage = 0;
  int humanColour = 1, preferredColour = 1;
  int botSquare = -1;
};
const char* name(pocketgames::Kind kind);
fui::Rect boardRect(const fui::DeviceContext& device);
int squareAt(const fui::DeviceContext& device, int count, int x, int y);
fui::Rect othelloBoardRect(const fui::DeviceContext& device);
void buildOthello(toybox::Screen&, const Model&);
void build(toybox::Screen& screen, const Model& model);
}  // namespace pocketui
