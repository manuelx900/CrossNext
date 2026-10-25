#include "PowerMenuActivity.h"

#include <I18n.h>

#include "../ui/PixelMenuTheme.h"
#include "PowerMenuScreen.h"
namespace powermenu {
namespace {
Action pending = Action::None;
}
void request(Action action) { pending = action; }
Action take() {
  const auto result = pending;
  pending = Action::None;
  return result;
}
}  // namespace powermenu
const char* PowerMenuActivity::headerTitle() const { return tr(STR_POWER_MENU); }
void PowerMenuActivity::buildScreen(UiScreen& s) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  powermenu::rows(items);
  freeink::ui::ListProps p;
  p.items = items;
  p.count = 3;
  p.action = ACTION_ROW;
  powermenu::place(s, p);
  syncListViewport(s, p);
  rows.bind(p, s.target(), s.body());
  pixelui::list(s, p);
}
void PowerMenuActivity::activateIndex(int n) {
  if (n < 0 || n > 2) return;
  chosen = static_cast<powermenu::Action>(n + 1);
  finish();
}
void PowerMenuActivity::onExit() {
  powermenu::pending = chosen;
  UiListActivity::onExit();
}
