#pragma once

#include "PetScreens.h"

namespace petui::roombackground {
// Room rasters emit gray runs through fill(); preserve thin floor seams on B/W panels.
class FloorInkTarget final : public fui::DrawTarget {
 public:
  FloorInkTarget(fui::DrawTarget& target, int floorTop) : target(target), floorTop(floorTop) {}
  fui::Rect clipRect() const override { return target.clipRect(); }
  bool setClipRect(fui::Rect r) override { return target.setClipRect(r); }
  fui::Size measureText(fui::FontId font, const char* value, fui::TextStyle style) const override {
    return target.measureText(font, value, style);
  }
  int16_t lineHeight(fui::FontId font) const override { return target.lineHeight(font); }
  uint64_t textMetricsKey(fui::FontId font, const char* value, fui::TextStyle style) const override {
    return target.textMetricsKey(font, value, style);
  }
  fui::Rect inkBounds(fui::FontId font, const char* value, fui::TextStyle style) const override {
    return target.inkBounds(font, value, style);
  }
  void fill(fui::Rect r, fui::Paint paint, uint8_t radius = 0, uint8_t corners = fui::CornersAll) override {
    if (paint.kind == fui::PaintKind::Dither && paint.color == fui::Color::LightGray && r.bottom() > floorTop) {
      const int bottom = r.bottom();
      if (r.y < floorTop) target.fill(fui::makeRect(r.x, r.y, r.width, floorTop - r.y), paint, radius, corners);
      r.y = std::max(int(r.y), floorTop);
      r.height = bottom - r.y;
      paint = fui::Paint::dither(fui::Color::DarkGray);
    }
    target.fill(r, paint, radius, corners);
  }
  void stroke(fui::Rect r, fui::Paint paint, uint8_t width, uint8_t radius = 0,
              uint8_t corners = fui::CornersAll) override {
    target.stroke(r, paint, width, radius, corners);
  }
  void line(fui::Point a, fui::Point b, uint8_t width, fui::Paint paint) override { target.line(a, b, width, paint); }
  void triangle(fui::Point a, fui::Point b, fui::Point c, fui::Paint paint) override {
    target.triangle(a, b, c, paint);
  }
  void text(fui::Rect r, const char* value, fui::TextStyle style) override { target.text(r, value, style); }
  void bitmap(fui::Rect r, fui::BitmapRef bitmap, fui::BitmapMode mode,
              fui::Paint paint = fui::Paint::solid(fui::Color::Black),
              fui::Rotation rotation = fui::Rotation::None) override {
    target.bitmap(r, bitmap, mode, paint, rotation);
  }

 private:
  fui::DrawTarget& target;
  int floorTop;
};
inline bool tiles(fui::DrawTarget& target, const Model& m, fui::Rect bounds, bool wide) {
  if (!m.fullRoomBackgrounds || !m.scenes.draw) return false;
  const unsigned first = wide ? ROOM_LANDSCAPE_ART : ROOM_PORTRAIT_ART;
  const int columns = wide ? 3 : 2, rows = wide ? 1 : 2;
  const auto clip = target.clipRect();
  FloorInkTarget floorInk(target, bounds.y + bounds.height * (wide ? 80 : 56) / 100);
  for (int row = 0; row < rows; ++row)
    for (int column = 0; column < columns; ++column) {
      const int left = bounds.x + column * bounds.width / columns;
      const int top = bounds.y + row * bounds.height / rows;
      const fui::Rect tile{int16_t(left), int16_t(top),
                           int16_t(bounds.x + (column + 1) * bounds.width / columns - left),
                           int16_t(bounds.y + (row + 1) * bounds.height / rows - top)};
      if (tile.right() <= clip.x || tile.x >= clip.right() || tile.bottom() <= clip.y || tile.y >= clip.bottom())
        continue;
      if (!m.scenes.draw(m.scenes.context, floorInk, tile, first + row * columns + column, false)) return false;
    }
  return true;
}
inline bool memory(fui::DrawTarget& target, const Model& m, fui::Rect crop, int floor) {
  // Snapshots share one room camera; the landscape panorama includes the menu's control area.
  constexpr int sourceW = 3, sourceH = 5;
  const int width = std::max(int(crop.width), (int(crop.height) * sourceW + sourceH - 1) / sourceH);
  const int height = (width * sourceH + sourceW - 1) / sourceW;
  const int top = std::clamp(floor - height * 56 / 100, int(crop.bottom()) - height, int(crop.y));
  return tiles(target, m, {int16_t(crop.x + (crop.width - width) / 2), int16_t(top), int16_t(width), int16_t(height)},
               false);
}
}  // namespace petui::roombackground
