// Real-font geometry and safety checks for the local SD wallpaper gallery.
#include <EpdFont.h>
#include <EpdFontFamily.h>
#include <FreeInkUI.h>
#include <I18n.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "../../src/apps_local/ui/ToyboxText.h"
#include "../../src/apps_local/ui/fonts/toybox_10.h"
#include "../../src/apps_local/ui/fonts/toybox_14.h"
#include "../../src/apps_local/ui/fonts/toybox_20.h"
#include "../../src/apps_local/ui/fonts/toybox_30.h"
#include "../../src/apps_local/wallpapers/WallpapersCore.h"
#include "../../src/apps_local/wallpapers/WallpapersScreens.h"
namespace fui = freeink::ui;
namespace {
int checks = 0, failed = 0;
void check(bool ok, const char* what) {
  ++checks;
  if (!ok) {
    ++failed;
    std::fprintf(stderr, "FAIL: %s\n", what);
  }
}
EpdFont small10(&toybox_10);
EpdFont button14(&toybox_14);
EpdFont ui20(&toybox_20);
EpdFont display30(&toybox_30);
EpdFontFamily smallFamily(&small10);
EpdFontFamily buttonFamily(&button14);
EpdFontFamily uiFamily(&ui20);
EpdFontFamily displayFamily(&display30);

class FontTarget final : public fui::DrawTarget {
 public:
  const EpdFontFamily* familyFor(const fui::FontId font) const {
    if (font == 3) return &smallFamily;
    if (font == fui::FONT_SLOT_SMALL) return &buttonFamily;
    if (font == fui::FONT_SLOT_BODY) return &uiFamily;
    return &displayFamily;
  }
  int widthOf(const fui::FontId font, const std::string& text) const {
    if (text.empty()) return 0;
    int w = 0;
    int h = 0;
    familyFor(font)->getTextDimensions(text.c_str(), &w, &h);
    return w;
  }
  fui::Size measureText(const fui::FontId font, const char* text, const fui::TextStyle) const override {
    return fui::Size{static_cast<int16_t>(widthOf(font, text == nullptr ? "" : text)), lineHeight(font)};
  }
  int16_t lineHeight(const fui::FontId font) const override {
    return static_cast<int16_t>(familyFor(font)->getData(EpdFontFamily::REGULAR)->advanceY);
  }
  void fill(fui::Rect, fui::Paint, uint8_t = 0, uint8_t = 0xFF) override {}
  void stroke(fui::Rect, fui::Paint, uint8_t, uint8_t = 0, uint8_t = 0xFF) override {}
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void text(fui::Rect, const char*, const fui::TextStyle) override {}
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint = {},
              fui::Rotation = fui::Rotation::None) override {}
};

bool overlap(fui::Rect a, fui::Rect b) {
  return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}
}  // namespace
int main() {
  FontTarget target;
  for (int w : {480, 800})
    for (bool selecting : {false, true}) {
      fui::DeviceContext d;
      d.width = w;
      d.height = w == 480 ? 800 : 480;
      d.hasTouch = true;
      const auto g = wallpapersui::gridGeom(d, selecting);
      check(g.perPage == 9, "nine local images per page");
      for (int n = 0; n < g.perPage; n++) {
        const auto cell = wallpapersui::thumbRect(g, n), marker = wallpapersui::selectionGeometry(cell).check;
        check(cell.x >= 0 && cell.right() <= d.width && cell.y >= 0 && cell.bottom() <= d.height,
              "thumbnail stays on screen");
        check(wallpapersui::cellAt(g, cell.x + cell.width / 2, cell.y + cell.height / 2) == n, "tap names drawn image");
        check(marker.x >= cell.x && marker.right() <= cell.right() && marker.y >= cell.y &&
                  marker.bottom() <= cell.bottom(),
              "selection stays inside image");
      }
      const auto keep = wallpapersui::confirmKeepRect(d), oldDelete = wallpapersui::sheetDeleteRect(d),
                 erase = wallpapersui::confirmDeleteRect(d);
      check(keep.x == oldDelete.x && keep.y == oldDelete.y && keep.width == oldDelete.width &&
                keep.height == oldDelete.height,
            "repeat delete tap cancels");
      check(!overlap(keep, erase), "delete requires different deliberate position");
      check(erase.bottom() <= d.height, "delete stays within panel");
      for (int count : {0, 1, 9, 10, 256})
        check(wallpapersui::pageCountFor(0, count, g.perPage) == std::max(1, (count + 8) / 9), "local-only page count");
    }
  for (auto lang : {Language::EN, Language::ES}) {
    I18N.setLanguage(lang);
    char text[128];
    std::snprintf(text, sizeof(text), tr(STR_WALL_SET_CONFIRM_FORMAT), 256);
    fui::TextStyle style;
    style.font = 1;
    style.maxLines = 4;
    check(toybox::fitLines(target, text, 412, 4, style) == text,
          "largest selection confirmation fits actual Jersey cut");
    style.font = fui::FONT_SLOT_SMALL;
    check(toybox::fitLines(target, tr(STR_WALL_EMPTY_SELECTION), 412, 4, style) == tr(STR_WALL_EMPTY_SELECTION),
          "empty-selection guidance fits the shared confirmation");
    for (const auto label : {StrId::STR_WALL_SELECT_ALL, StrId::STR_WALL_CLEAR_SELECTION}) {
      check(target.widthOf(style.font, I18N.get(label)) <= 72, "selection toggle label fits the compact footer");
    }
    for (const auto label : {StrId::STR_WALL_CHOOSE_IMAGES, StrId::STR_WALL_NO_WALLPAPER})
      check(target.widthOf(style.font, I18N.get(label)) <= 184, "empty-selection actions fit the confirmation");
  }
  std::printf("wallcaption: %d checks, %d failed\n", checks, failed);
  return failed ? 1 : 0;
}
