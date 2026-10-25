#pragma once
#include "GamePanel.h"

namespace frontdoor {
namespace fui = freeink::ui;
enum class Symbol : uint8_t { Play, Restart, Grid, Help, Nearby, Settings };
struct Row {
  const char* label = nullptr;
  fui::ActionId action = fui::NO_ACTION;
  int16_t value = 0;
  Symbol symbol = Symbol::Play;
  const char* detail = nullptr;
};
struct Layout {
  fui::Rect heading{}, detail{}, preview{}, footnote{}, rows[5]{};
};
Layout layout(const fui::DeviceContext&, int rows, bool detail = false, bool footnote = false, bool compact = false);
void begin(toybox::Screen&, const char* title, const char* right = nullptr);
void text(toybox::Screen&, fui::Rect, const char*, int font = 0, bool centered = true, int lines = 1);
void rows(toybox::Screen&, const Layout&, const Row*, int count, int selected = -1);
}  // namespace frontdoor
