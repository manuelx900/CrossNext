#pragma once

#include <I18n.h>

#include "../ui/ToyboxScreen.h"

namespace wallpapersui {
namespace fui = freeink::ui;

struct GridGeom {
  int cols = 2;     // columns per page
  int rows = 2;     // rows per page
  int perPage = 4;  // cols * rows
  int16_t cellW = 0;
  int16_t cellH = 0;       // the thumbnail area only
  int16_t markerRoom = 0;  // clearance under the thumbnail for the selection marker
  int16_t captionH = 0;    // the name, below the marker's clearance
  int16_t originX = 0;     // top-left of slot 0
  int16_t originY = 0;
  int16_t gapX = 0;
  int16_t gapY = 0;
  int16_t pageDotsY = 0;  // where the page-dot strip sits (when more than one page)
};

GridGeom gridGeom(const fui::DeviceContext& device, bool selecting = false);
fui::Rect thumbRect(const GridGeom& g, int slot);
fui::Rect cellRect(const GridGeom& g, int slot);
fui::Rect captionRect(const GridGeom& g, int slot);
int cellAt(const GridGeom& g, int x, int y);
int pageCountFor(int specialTiles, int libraryCount, int perPage);
uint32_t gridMeaning(int page, int view, int libraryCount, int specialTiles, bool choosing);

struct SelectionGeometry {
  fui::Rect frame, check;
};

SelectionGeometry selectionGeometry(const fui::Rect& thumb);

enum : fui::ActionId {
  ActionDismiss = 4,
  ActionDelete = 6,
  ActionKeep = 7,
  ActionConfirmDelete = 8,
  ActionSet = 18,
  ActionSlideshow = 19,
  ActionExitPreview = 20,
};

// Repeated activation of the sheet's Delete cancels its confirmation.
constexpr int16_t kSheetButtonH = 64;
fui::Rect sheetHeadRect(const fui::DeviceContext& device);
fui::Rect sheetProseRect(const fui::DeviceContext& device);
fui::Rect confirmProseRect(const fui::DeviceContext& device);
fui::Rect contextRect(const fui::DeviceContext& device);
fui::Rect contextRowRect(const fui::DeviceContext& device, int row);
fui::Rect sheetPreviewRect(const fui::DeviceContext& device);
fui::Rect sheetDeleteRect(const fui::DeviceContext& device);
fui::Rect confirmKeepRect(const fui::DeviceContext& device);
fui::Rect confirmDeleteRect(const fui::DeviceContext& device);

struct SheetModel {
  const char* name = "";  // the wallpaper's display name, not its file name
  const char* detail = "";
  bool isActive = false;  // it is the one currently on the sleep screen
};
void buildSheet(toybox::Screen& screen, const SheetModel& model);

struct ConfirmModel {
  const char* name = "";
  const char* consequence = "";
};
void buildConfirm(toybox::Screen& screen, const ConfirmModel& model);

struct NoticeModel {
  const char* headline = "";
  const char* body = "";
  const char* actionLabel = nullptr;
  fui::ActionId action = 0;
};
void buildNotice(toybox::Screen& screen, const NoticeModel& model);

struct GridChromeModel {
  bool selecting = false;
  int selectedCount = 0;
  const char* title = tr(STR_UI_WALLPAPERS_2AA278);
  const char* rightLabel = nullptr;
  const char* warning = nullptr;  // free-space advisory, null when there is room
  bool hasActive = false;         // false -> draw the "tap one to set it" hint
  // A note can supply the current suspension screen.
  bool noteOn = false;
  // The sleep-screen line (#354). Carries every fact that applies at once: what
  // the last selection changed behind the user's back, AND any standing caveat
  // about the wallpaper not reaching the glass. Built by
  // wallpapers::stripLineAfterSelection / reachHint, which are what guarantee
  // one cannot hide the other. Wins the strip -- see buildGridChrome.
  const char* note = nullptr;
};

const char* noteStripLine();
void buildGridChrome(toybox::Screen& screen, const GridChromeModel& model);
int16_t hintTextWidth(const fui::Rect& safe);
int16_t hintStripHeight();

struct EmptyModel {
  const char* title = tr(STR_UI_WALLPAPERS_2AA278);
  const char* warning = nullptr;
};
void buildEmpty(toybox::Screen& screen, const EmptyModel& model);
}  // namespace wallpapersui
