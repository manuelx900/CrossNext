#include "LibraryEntryBackdrop.h"

#include <FreeInkUIIcon.h>

#include "LibraryControls.h"
#include "apps_local/ui/NavigationIcons.h"
#include "apps_local/ui/PageIndicator.h"
#include "components/UITheme.h"

namespace libraryui {
freeink::ui::Rect entryBackdrop(const GfxRenderer& renderer, freeink::ui::DrawTarget& target,
                                const freeink::ui::DeviceContext& device, freeink::ui::Rect body, bool covers,
                                int bookCount, int selected) {
  namespace fui = freeink::ui;
  if (body.width <= 0 || body.height <= 0) return body;
  if (covers) {
    const auto grid = gridLayout(device, body);
    const int first = std::clamp(selected, 0, std::max(0, bookCount - 1)) / grid.capacity() * grid.capacity();
    for (int n = 0; n < std::clamp(bookCount - first, 0, grid.capacity()); ++n) {
      const auto card = grid.card(n);
      GUI.drawCoverPlaceholder(renderer, Rect{card.x, card.y, card.width, card.height});
    }
    if (bookCount >= 0)
      pageui::draw(target, grid.pager, first / grid.capacity() + 1,
                   (bookCount + grid.capacity() - 1) / grid.capacity());
    return grid.area;
  }
  return body;
}
}  // namespace libraryui
