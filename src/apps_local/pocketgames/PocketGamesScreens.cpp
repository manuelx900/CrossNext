#include "PocketGamesScreens.h"

#include <I18n.h>

namespace pocketui {
namespace pg = pocketgames;
const char* name(pg::Kind) { return tr(STR_REDESIGN_OTHELLO); }
fui::Rect boardRect(const fui::DeviceContext& device) { return othelloBoardRect(device); }
int squareAt(const fui::DeviceContext& device, int count, int x, int y) {
  const auto b = boardRect(device);
  if (count <= 0 || x < b.x || y < b.y || x >= b.right() || y >= b.bottom()) return -1;
  return (y - b.y) * count / b.height * count + (x - b.x) * count / b.width;
}
void build(toybox::Screen& s, const Model& m) {
  if (m.state) buildOthello(s, m);
}
}  // namespace pocketui
