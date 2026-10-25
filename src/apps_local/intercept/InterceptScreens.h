#pragma once
#include "../ui/PagedText.h"
#include "../ui/SdBitmapPack.h"
#include "../ui/ToyboxScreen.h"
#include "InterceptCore.h"
namespace interceptui {
enum class View : uint8_t { Menu, Story, Restart, About };
enum : freeink::ui::ActionId { Play = 1, New, ConfirmNew, Cancel, About, Choice, Page, Back };
struct Model {
  const intercept::State* state = nullptr;
  View view = View::Menu;
  bool failed = false;
  int selected = 0, page = 0;
  sdart::Pack* assets = nullptr;
};
struct Work {
  int pages = 1;
  freeink::ui::ButtonProps button;
  pagedtext::Work paragraph;
  char label[192]{};
};
void build(toybox::Screen&, const Model&, Work&);
}  // namespace interceptui
