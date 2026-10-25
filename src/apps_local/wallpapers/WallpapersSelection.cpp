#include "WallpapersSelection.h"

#include <I18n.h>

#include <cstdio>

#include "../ui/Dashboard.h"
#include "../ui/PageIndicator.h"

namespace wallpapersui {
namespace fui = freeink::ui;
namespace {
constexpr int kSelectionButtonWidth = 88;
constexpr int kSelectionSideReserve = kSelectionButtonWidth + 18;
}  // namespace
fui::Rect galleryPager(const fui::DeviceContext& device, bool selecting) {
  const auto safe = device.safeRect();
  const int reserve = selecting ? kSelectionSideReserve : 10;
  return fui::makeRect(safe.x + reserve, safe.bottom() - 60, safe.width - 2 * reserve, 44);
}
void selectionFooter(toybox::Screen& screen, bool selecting, int page, int pages, bool allSelected) {
  const auto safe = screen.device().safeRect();
  pageui::build(screen, galleryPager(screen.device(), selecting), page + 1, pages, ActionGalleryPage, -1,
                ActionGalleryPage, 1);
  if (!selecting) return;
  dashboard::button(screen, fui::makeRect(safe.x + 10, safe.bottom() - 60, kSelectionButtonWidth, 44),
                    allSelected ? tr(STR_WALL_CLEAR_SELECTION) : tr(STR_WALL_SELECT_ALL), ActionSelectAll);
  dashboard::button(
      screen, fui::makeRect(safe.right() - 10 - kSelectionButtonWidth, safe.bottom() - 60, kSelectionButtonWidth, 44),
      tr(STR_DONE), ActionSelectionDone);
}
void selectionConfirm(toybox::Screen& screen, int count) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_WALLPAPERS_TITLE);
  header.back = screen.device().hasTouch;
  pixelui::header(screen, header);
  const auto body = pixelui::geometry(screen.device()).body;
  const int height = std::min(280, int(body.height) - 20);
  const auto box = fui::makeRect(body.x + 8, body.y + (body.height - height) / 2, body.width - 16, height);
  dashboard::frame(screen, box, pixelui::kBorder);
  char text[128];
  if (count > 0)
    std::snprintf(text, sizeof(text), tr(STR_WALL_SET_CONFIRM_FORMAT), count);
  else
    std::snprintf(text, sizeof(text), "%s", tr(STR_WALL_EMPTY_SELECTION));
  fui::TextStyle style;
  style.font = 0;
  style.align = fui::TextAlign::Center;
  style.maxLines = 4;
  screen.target().text(fui::makeRect(box.x + 16, box.y + 20, box.width - 32, box.height - 102), text, style);
  const int half = (box.width - 44) / 2;
  dashboard::button(screen, fui::makeRect(box.x + 16, box.bottom() - 72, half, 54),
                    count > 0 ? tr(STR_CANCEL) : tr(STR_WALL_CHOOSE_IMAGES), ActionSelectionCancel);
  dashboard::button(screen, fui::makeRect(box.right() - 16 - half, box.bottom() - 72, half, 54),
                    count > 0 ? tr(STR_WALL_SET_APPLY) : tr(STR_WALL_NO_WALLPAPER), ActionSelectionApply);
}
void preparation(toybox::Screen& screen, int done, int total, bool applying, unsigned frame) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_WALLPAPERS_TITLE);
  header.back = !applying && screen.device().hasTouch;
  pixelui::header(screen, header);
  const auto body = pixelui::geometry(screen.device()).body;
  const int center = body.y + body.height / 2;
  pixelui::text(screen.target(), fui::makeRect(body.x, center - 100, body.width, 50), tr(STR_WALL_PREPARE_TITLE), 1,
                fui::Color::Black, fui::TextAlign::Center);
  char text[96];
  if (total > 0)
    std::snprintf(text, sizeof(text), applying ? tr(STR_WALL_SAVING_FORMAT) : tr(STR_WALL_PROCESSING_FORMAT), done,
                  total);
  else
    std::snprintf(text, sizeof(text), "%s", tr(STR_WALL_SCANNING));
  fui::TextStyle style;
  style.font = 0;
  style.align = fui::TextAlign::Center;
  style.maxLines = 2;
  screen.target().text(fui::makeRect(body.x, center - 40, body.width, 64), text, style);
  const auto ink = fui::Paint::solid(fui::Color::Black);
  for (unsigned n = 0; n < 4; ++n) {
    const auto dot = fui::makeRect(body.x + body.width / 2 - 48 + n * 26, center + 60, 18, 18);
    screen.target().stroke(dot, ink, 2);
    if (n == frame % 4) screen.target().fill(dot, ink);
  }
  if (total > 0) {
    const auto bar = fui::makeRect(body.x + 20, center + 104, body.width - 40, 16);
    screen.target().stroke(bar, ink, 2);
    screen.target().fill(fui::makeRect(bar.x + 4, bar.y + 4, (bar.width - 8) * std::clamp(done, 0, total) / total, 8),
                         ink);
  }
  if (!applying)
    dashboard::button(screen, fui::makeRect(body.x, body.bottom() - 56, body.width, 52), tr(STR_CANCEL),
                      ActionPrepareCancel);
}
}  // namespace wallpapersui
