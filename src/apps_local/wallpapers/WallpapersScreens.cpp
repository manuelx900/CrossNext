#include "WallpapersScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "../ui/ExtraIcons.h"
#include "../ui/PixelMenu.h"
#include "../ui/ToyboxText.h"
#include "WallpapersSelection.h"

namespace wallpapersui {
namespace {
constexpr int16_t kHintH = 54;
constexpr int16_t kPageStripH = 76;
constexpr int16_t kBottomMargin = 0;
constexpr int16_t kGap = 10;
constexpr int kRows = 3;
constexpr int16_t kCaptionH = 0;
constexpr int16_t kMarkerRoom = 0;

// Shared headline height for the contextual menu and deletion confirmation.
constexpr int16_t kSheetHeadH = 44;

// Three by four cards match the reference gallery; source images retain their
// own aspect and are fitted inside each card by the thumbnail decoder.
constexpr float kCellAspectWoverH = 3.0f / 4.0f;

fui::TextStyle owned(fui::TextStyle style, fui::TextAlign align) {
  style.align = align;
  return style;
}

fui::TextStyle onPaper(fui::TextStyle style, fui::TextAlign align, uint8_t maxLines = 0) {
  style.align = align;
  style.color = fui::Color::Black;
  if (maxLines > 0) style.maxLines = maxLines;
  return style;
}

constexpr int16_t kButtonH = 64;

void drawButton(toybox::Screen& screen, const fui::Rect& box, const char* label, const fui::ActionId action,
                const fui::Rect* hit = nullptr) {
  lightui::surface(screen.target(), box, fui::Paint::solid(fui::Color::Black));
  pixelui::text(screen.target(), box, label, 0, fui::Color::White, fui::TextAlign::Center);
  screen.frame().hit(hit == nullptr ? box : *hit, action);
}

void drawProse(toybox::Screen& screen, const fui::Rect& box, const char* text, const fui::TextAlign align) {
  fui::TextStyle style = onPaper(screen.theme().bodyText, align);
  style.font = 4;
  const int16_t lineH = screen.target().lineHeight(style.font);
  const int lines = lineH > 0 ? box.height / lineH : 1;
  style.maxLines = static_cast<uint8_t>(lines < 1 ? 1 : (lines > 16 ? 16 : lines));
  screen.target().text(box, text, style);
}

void chrome(toybox::Screen& screen, const char* title, const char* rightLabel) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.rightLabel = rightLabel;
  header.back = screen.device().hasTouch;
  pixelui::header(screen, header);
}

}  // namespace

GridGeom gridGeom(const fui::DeviceContext& device, bool selecting) {
  const fui::Rect safe = device.safeRect();
  GridGeom g;
  g.cols = device.width > device.height ? 4 : 3;
  g.rows = device.width > device.height ? 1 : kRows;
  g.perPage = g.cols * g.rows;
  g.captionH = kCaptionH;
  g.markerRoom = kMarkerRoom;
  g.gapX = kGap;
  g.gapY = kGap;

  const int16_t gridLeft = static_cast<int16_t>(safe.x + toybox::kMargin);
  const int16_t gridW = static_cast<int16_t>(safe.width - toybox::kMargin * 2);
  // Sides and bottom off the safe rect; the top absolute, because the header
  // band already covers the rows the glass hides. See toybox::kBodyTop.
  const int16_t gridTop = pixelui::geometry(device).body.y + (selecting ? kHintH : 0) + 8;
  const int16_t gridBottom = static_cast<int16_t>(safe.bottom() - kPageStripH - kBottomMargin);
  const int16_t gridH = static_cast<int16_t>(gridBottom - gridTop);

  // The largest a cell may be in each axis, then the wallpaper's aspect fitted
  // inside that box so the thumbnail is never stretched.
  const int16_t maxCellW = static_cast<int16_t>((gridW - g.gapX * (g.cols - 1)) / g.cols);
  const int16_t maxCellH = static_cast<int16_t>(
      (gridH - (selecting ? 0 : kHintH) - g.gapY * (g.rows - 1) - (g.captionH + g.markerRoom) * g.rows) / g.rows);
  int16_t cellW = std::min<int16_t>(maxCellW, static_cast<int16_t>(maxCellH * kCellAspectWoverH));
  if (cellW < 1) cellW = 1;
  int16_t cellH = static_cast<int16_t>(cellW / kCellAspectWoverH);
  if (cellH > maxCellH) {
    cellH = maxCellH;
    cellW = static_cast<int16_t>(cellH * kCellAspectWoverH);
  }
  g.cellW = cellW;
  g.cellH = cellH;

  // Centre the columns horizontally; top-align the rows.
  const int16_t usedW = static_cast<int16_t>(g.cols * cellW + (g.cols - 1) * g.gapX);
  g.originX = static_cast<int16_t>(gridLeft + (gridW - usedW) / 2);
  const int usedH = g.rows * (cellH + g.captionH + g.markerRoom) + (g.rows - 1) * g.gapY;
  g.originY = gridTop + std::max(0, (gridH - usedH) / 2);
  g.pageDotsY = static_cast<int16_t>(safe.bottom() - kPageStripH + 16);
  return g;
}

fui::Rect previewRect(int pageWidth, int pageHeight, int imageWidth, int imageHeight) {
  if (pageWidth <= 0 || pageHeight <= 0 || imageWidth <= 0 || imageHeight <= 0) return {};
  int width = std::min(pageWidth, imageWidth);
  int height = std::max(1, static_cast<int>(int64_t(imageHeight) * width / imageWidth));
  if (height > pageHeight) {
    height = std::min(pageHeight, imageHeight);
    width = std::max(1, static_cast<int>(int64_t(imageWidth) * height / imageHeight));
  }
  return fui::makeRect((pageWidth - width) / 2, (pageHeight - height) / 2, width, height);
}

fui::Rect thumbRect(const GridGeom& g, int slot) {
  const int col = slot % g.cols;
  const int row = slot / g.cols;
  const int16_t x = static_cast<int16_t>(g.originX + col * (g.cellW + g.gapX));
  const int16_t y = static_cast<int16_t>(g.originY + row * (g.cellH + g.markerRoom + g.captionH + g.gapY));
  return fui::makeRect(x, y, g.cellW, g.cellH);
}

fui::Rect cellRect(const GridGeom& g, int slot) {
  const fui::Rect t = thumbRect(g, slot);
  return fui::makeRect(t.x, t.y, t.width, static_cast<int16_t>(t.height + g.markerRoom + g.captionH));
}

fui::Rect captionRect(const GridGeom& g, int slot) {
  if (g.captionH <= 0) return fui::makeRect(0, 0, 0, 0);
  const fui::Rect t = thumbRect(g, slot);
  return fui::makeRect(t.x, static_cast<int16_t>(t.y + t.height + g.markerRoom), t.width, g.captionH);
}

int cellAt(const GridGeom& g, int x, int y) {
  for (int slot = 0; slot < g.perPage; ++slot) {
    const fui::Rect c = cellRect(g, slot);
    if (x >= c.x && x < c.right() && y >= c.y && y < c.bottom()) return slot;
  }
  return -1;
}

int pageCountFor(const int specialTiles, const int libraryCount, const int perPage) {
  if (perPage <= 0) return 1;
  const int tiles = (specialTiles < 0 ? 0 : specialTiles) + (libraryCount < 0 ? 0 : libraryCount);
  if (tiles <= 0) return 1;
  return (tiles + perPage - 1) / perPage;
}

uint32_t gridMeaning(const int page, const int view, const int libraryCount, const int specialTiles,
                     const bool choosing) {
  uint32_t m = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(page));
  m = paintclock::mixMeaning(m, static_cast<uint32_t>(view));
  m = paintclock::mixMeaning(m, static_cast<uint32_t>(libraryCount));
  m = paintclock::mixMeaning(m, static_cast<uint32_t>(specialTiles));
  return paintclock::mixMeaning(m, choosing ? 1u : 0u);
}

int16_t hintTextWidth(const fui::Rect& safe) { return static_cast<int16_t>(safe.width - toybox::kMargin * 2); }

int16_t hintStripHeight() { return kHintH; }

const char* noteStripLine() { return tr(STR_UI_A_NOTE_IS_YOUR_SLEEP_SCREEN_C84D7F); }

void buildGridChrome(toybox::Screen& screen, const GridChromeModel& model) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_WALLPAPERS_TITLE);
  header.back = screen.device().hasTouch;
  header.mark = fui::bitmapFromIcon(extra_dashboard_cards_32);
  header.markAction = ActionSelectSet;
  pixelui::header(screen, header);
  if (!model.selecting) return;
  const auto safe = screen.frame().safeRect();
  const char* hint = model.note && model.note[0]         ? model.note
                     : model.warning && model.warning[0] ? model.warning
                     : model.noteOn                      ? noteStripLine()
                                                         : "";
  fui::TextStyle style = onPaper(screen.theme().smallText, fui::TextAlign::Center);
  style.font = pixelui::kSmall;
  style.maxLines = 2;
  char selected[96];
  if (model.selecting) {
    std::snprintf(selected, sizeof(selected), tr(STR_WALL_SELECT_HINT_FORMAT), model.selectedCount);
    hint = selected;
  }
  screen.target().text(
      fui::makeRect(safe.x + toybox::kMargin, pixelui::geometry(screen.device()).body.y, hintTextWidth(safe), kHintH),
      hint, style);
}

void buildEmpty(toybox::Screen& screen, const EmptyModel& model) {
  chrome(screen, model.title, nullptr);

  if (model.warning != nullptr && model.warning[0] != '\0') {
    fui::TextStyle warn = onPaper(screen.theme().smallText, fui::TextAlign::Left);
    std::string fitted = toybox::fittedTitle(screen.target(), model.warning, screen.body().width, warn);
    screen.target().text(screen.takeTop(30, toybox::kGutter), fitted.c_str(), warn);
  }

  pixelui::text(screen.target(), screen.takeTop(52, toybox::kGutter), tr(STR_UI_NO_WALLPAPERS_3C7751), 1);

  fui::TextStyle detail = owned(screen.theme().bodyText, fui::TextAlign::Left);
  detail.maxLines = 6;
  screen.target().text(screen.body().inset(fui::Insets{12, 12, 12, 12}), tr(STR_WALLPAPER_FOLDERS), detail);
}

SelectionGeometry selectionGeometry(const fui::Rect& thumb) {
  const int side = std::min(26, int(thumb.width) / 4);
  return {thumb, fui::makeRect(thumb.right() - side - 5, thumb.bottom() - side - 5, side, side)};
}

void buildNotice(toybox::Screen& screen, const NoticeModel& model) {
  chrome(screen, tr(STR_UI_WALLPAPERS_2AA278), nullptr);
  const fui::Rect body = screen.body();
  const int contentBottom = model.actionLabel ? body.bottom() - kButtonH - 16 : body.bottom();
  pixelui::text(screen.target(), fui::makeRect(body.x, body.y + 16, body.width, 52), model.headline, 1);
  drawProse(screen, fui::makeRect(body.x, body.y + 80, body.width, std::max(0, contentBottom - body.y - 80)),
            model.body, fui::TextAlign::Left);

  if (model.actionLabel != nullptr) {
    drawButton(screen, fui::makeRect(body.x, body.bottom() - kButtonH, body.width, kButtonH), model.actionLabel,
               model.action);
  }
}

fui::Rect contextRect(const fui::DeviceContext& device) {
  const auto safe = device.safeRect();
  const int width = std::min(600, int(safe.width) - 48);
  const int height = std::min(328, int(safe.height) - 128);
  return fui::makeRect(safe.x + (safe.width - width) / 2, safe.y + (safe.height - height) / 2, width, height);
}

fui::Rect contextRowRect(const fui::DeviceContext& device, int row) {
  const auto box = contextRect(device);
  const int height = (box.height - 96) / 4;
  return fui::makeRect(box.x + 12, box.y + 88 + row * height, box.width - 24, height);
}

fui::Rect sheetHeadRect(const fui::DeviceContext& device) {
  const fui::Rect safe = device.safeRect();
  return fui::makeRect(static_cast<int16_t>(safe.x + toybox::kMargin), static_cast<int16_t>(toybox::kChromeHeight + 8),
                       static_cast<int16_t>(safe.width - toybox::kMargin * 2), kSheetHeadH);
}

namespace {
fui::Rect proseTo(const fui::DeviceContext& device, const fui::Rect& firstControl) {
  const fui::Rect head = sheetHeadRect(device);
  const int16_t top = static_cast<int16_t>(head.bottom() + toybox::kGutter);
  const int16_t bottom = static_cast<int16_t>(firstControl.y - toybox::kGutter);
  return fui::makeRect(head.x, top, head.width, static_cast<int16_t>(bottom > top ? bottom - top : 0));
}

}  // namespace

fui::Rect sheetPreviewRect(const fui::DeviceContext& device) { return contextRowRect(device, 0); }

fui::Rect sheetDeleteRect(const fui::DeviceContext& device) { return contextRowRect(device, 1); }

fui::Rect confirmKeepRect(const fui::DeviceContext& device) { return sheetDeleteRect(device); }

fui::Rect confirmDeleteRect(const fui::DeviceContext& device) {
  const auto safe = device.safeRect();
  return fui::makeRect(safe.x + toybox::kMargin, safe.bottom() - 64, safe.width - 2 * toybox::kMargin, 56);
}

fui::Rect sheetProseRect(const fui::DeviceContext& device) { return proseTo(device, sheetPreviewRect(device)); }

fui::Rect confirmProseRect(const fui::DeviceContext& device) { return proseTo(device, confirmKeepRect(device)); }

namespace {
void drawSheetName(toybox::Screen& screen, const fui::Rect& box, const char* name) {
  char label[128];
  size_t length = std::min(std::strlen(name), sizeof(label) - 1);
  while (length && (static_cast<unsigned char>(name[length]) & 0xC0) == 0x80) --length;
  std::memcpy(label, name, length);
  label[length] = 0;
  fui::TextStyle style;
  style.font = 0;
  style.maxLines = 1;
  pixelui::fitLabel(screen.target(), label, box.width, style);
  screen.target().text(box, label, style);
}

}  // namespace

void buildSheet(toybox::Screen& screen, const SheetModel& model) {
  const auto device = screen.device();
  const auto box = contextRect(device);
  auto& target = screen.target();
  const auto ink = fui::Paint::solid(fui::Color::Black), paper = fui::Paint::solid(fui::Color::White);
  lightui::surface(target, box, paper);
  lightui::outline(target, box, ink, 2);
  fui::TextStyle title;
  title.font = 0;
  title.maxLines = 1;
  drawSheetName(screen, fui::makeRect(box.x + 16, box.y + 12, box.width - 32, 32), model.name);
  title.font = 3;
  target.text(fui::makeRect(box.x + 16, box.y + 48, box.width - 32, 28), model.detail, title);
  const char* labels[] = {tr(STR_WALLPAPER_SET), tr(STR_UI_DELETE_D6F563), tr(STR_WALLPAPER_SLIDESHOW),
                          tr(STR_WALLPAPER_EXIT_PREVIEW)};
  static constexpr fui::ActionId actions[] = {ActionSet, ActionDelete, ActionSlideshow, ActionExitPreview};
  for (int n = 0; n < 4; ++n) {
    const auto row = contextRowRect(device, n);
    target.line({row.x, row.y}, {int16_t(row.right()), row.y}, 1, ink);
    const auto icon = fui::makeRect(row.x + 4, row.y + (row.height - 32) / 2, 32, 32);
    if (n == 0) {
      target.stroke(icon, ink, 2);
      target.fill(fui::makeRect(icon.x + 22, icon.y + 6, 5, 5), ink);
      target.triangle({int16_t(icon.x + 4), int16_t(icon.y + 26)}, {int16_t(icon.x + 12), int16_t(icon.y + 14)},
                      {int16_t(icon.x + 22), int16_t(icon.y + 26)}, ink);
      target.triangle({int16_t(icon.x + 14), int16_t(icon.y + 26)}, {int16_t(icon.x + 22), int16_t(icon.y + 19)},
                      {int16_t(icon.x + 28), int16_t(icon.y + 26)}, ink);
    } else if (n == 1) {
      target.stroke(fui::makeRect(icon.x + 10, icon.y, 12, 6), ink, 2);
      target.fill(fui::makeRect(icon.x, icon.y + 6, 32, 4), ink);
      target.stroke(fui::makeRect(icon.x + 3, icon.y + 10, 26, 22), ink, 3);
      target.fill(fui::makeRect(icon.x + 10, icon.y + 14, 3, 12), ink);
      target.fill(fui::makeRect(icon.x + 19, icon.y + 14, 3, 12), ink);
    } else if (n == 2) {
      target.stroke(fui::makeRect(icon.x + 6, icon.y, 26, 26), ink, 2);
      const auto front = fui::makeRect(icon.x, icon.y + 6, 26, 26);
      target.fill(front, paper);
      target.stroke(front, ink, 2);
      target.triangle({int16_t(icon.x + 8), int16_t(icon.y + 12)}, {int16_t(icon.x + 8), int16_t(icon.y + 26)},
                      {int16_t(icon.x + 20), int16_t(icon.y + 19)}, ink);
    } else {
      target.stroke(icon, ink, 2);
      target.line({int16_t(icon.x + 7), int16_t(icon.y + 7)}, {int16_t(icon.right() - 7), int16_t(icon.bottom() - 7)},
                  3, ink);
      target.line({int16_t(icon.right() - 7), int16_t(icon.y + 7)}, {int16_t(icon.x + 7), int16_t(icon.bottom() - 7)},
                  3, ink);
    }
    pixelui::text(target, fui::makeRect(row.x + 48, row.y + 8, row.width - 56, 42), labels[n], 0);
    screen.frame().hit(row, actions[n], 0, fui::InputTouch);
  }
}

void buildConfirm(toybox::Screen& screen, const ConfirmModel& model) {
  chrome(screen, tr(STR_UI_DELETE_WALLPAPER_2258AD), nullptr);
  const fui::DeviceContext device = screen.frame().device();

  drawSheetName(screen, sheetHeadRect(device), model.name);

  fui::TextStyle message;
  message.font = device.width > device.height ? 3 : 1;
  message.maxLines = 3;
  screen.target().text(confirmProseRect(device), model.consequence, message);

  // KEEP IT first, and on the pixels the sheet's DELETE occupied. A repeat of
  // the press that got here is a cancel, by construction rather than by luck.
  drawButton(screen, confirmKeepRect(device), tr(STR_UI_KEEP_IT_BCFE52), ActionKeep);
  drawButton(screen, confirmDeleteRect(device), tr(STR_UI_DELETE_IT_619935), ActionConfirmDelete);
}

}  // namespace wallpapersui
