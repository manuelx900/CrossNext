#pragma once

#include "PetHabitat.h"
#include "PetRoomBackground.h"

namespace petui::roomview {
using namespace habitatlayout;
inline constexpr auto ink = fui::Paint::solid(fui::Color::Black);
inline constexpr auto paper = fui::Paint::solid(fui::Color::White);

struct Layout {
  fui::Rect bounds, scene;
  int floorY;
};
inline Layout layout(fui::Rect bounds, fui::Rect picture, fui::Rect controls) {
  const bool wide = bounds.width > bounds.height;
  const int width = (wide ? picture.width : bounds.width) & ~1;
  const int horizon = wide ? 80 : 56;
  int floor = bounds.y + bounds.height * horizon / 100;
  if (!wide) floor = std::min(floor, controls.y - width * 55 / 432 - 20);
  return {bounds, rect(wide ? picture.x : bounds.x, floor - width * 135 / 432, width, width / 2), floor};
}
inline void window(const Model& m, fui::DrawTarget& target, fui::Rect scene) {
  const auto box = scale(scene, rect(338, 10, 80, 68));
  const auto previous = target.clipRect();
  const int left = std::max(int(box.x), int(previous.x)), top = std::max(int(box.y), int(previous.y));
  const int right = std::min(int(box.right()), int(previous.right()));
  const int bottom = std::min(int(box.bottom()), int(previous.bottom()));
  bool drawn = false;
  if (right > left && bottom > top && target.setClipRect(rect(left, top, right - left, bottom - top))) {
    drawn = m.habitat.draw &&
            m.habitat.draw(m.habitat.context, target,
                           rect(scene.x + scene.width / 2, scene.y, scene.width / 2, scene.height), 1, false);
    target.setClipRect(previous);
  }
  if (!drawn) {
    const auto frame = box.inset({3, 3, 3, 3});
    target.fill(frame, paper);
    target.stroke(frame, ink, 2);
    target.line({int16_t(frame.x + frame.width / 2), frame.y}, {int16_t(frame.x + frame.width / 2), frame.bottom()}, 1,
                ink);
    target.line({frame.x, int16_t(frame.y + frame.height / 2)}, {frame.right(), int16_t(frame.y + frame.height / 2)}, 1,
                ink);
  }
}
inline void backdrop(fui::DrawTarget& target, const Layout& room) {
  const auto r = room.bounds;
  target.fill(r, paper);
  const int inset = std::max(10, int(r.width) / 40), left = r.x + inset, right = r.right() - inset;
  const auto texture = fui::Paint::dither(fui::Color::LightGray);
  for (int y = r.y + 58, row = 0; y < room.floorY - 8; y += 24, ++row)
    for (int x = left + 12 + row % 2 * 16; x < right - 8; x += 32) target.fill(rect(x, y, 2, 1), texture);
  for (int offset : {0, 4}) {
    target.line({int16_t(left + offset), int16_t(r.y + 6)}, {int16_t(left + offset), int16_t(room.floorY + 10)}, 1,
                ink);
    target.line({int16_t(right - offset), int16_t(r.y + 6)}, {int16_t(right - offset), int16_t(room.floorY + 10)}, 1,
                ink);
    target.line({int16_t(left), int16_t(r.y + 6 + offset)}, {int16_t(right), int16_t(r.y + 6 + offset)}, 1, ink);
    target.line({int16_t(left), int16_t(room.floorY + offset)}, {int16_t(right), int16_t(room.floorY + offset)}, 1,
                ink);
  }
  target.line({r.x, r.y}, {int16_t(left), int16_t(r.y + 6)}, 1, ink);
  target.line({r.right(), r.y}, {int16_t(right), int16_t(r.y + 6)}, 1, ink);
  target.line({r.x, int16_t(room.floorY + 16)}, {int16_t(left), int16_t(room.floorY)}, 1, ink);
  target.line({r.right(), int16_t(room.floorY + 16)}, {int16_t(right), int16_t(room.floorY)}, 1, ink);
  int row = 0;
  for (int y = room.floorY + 12; y < r.bottom(); ++row) {
    const int height = std::max(14, int(r.width) / 32) + std::min(row, 12);
    const int end = std::min(int(r.bottom()) - 1, y + height);
    target.line({r.x, int16_t(y)}, {r.right(), int16_t(y)}, 1, ink);
    const int plank = std::max(70, int(r.width) / 3);
    for (int x = r.x + (row % 3 + 1) * plank / 3; x < r.right(); x += plank) {
      target.line({int16_t(x), int16_t(y)}, {int16_t(x - 3), int16_t(end)}, 1, ink);
      target.line({int16_t(std::min(x + 12, int(r.right()) - 1)), int16_t((y + end) / 2)},
                  {int16_t(std::min(x + 20, int(r.right()) - 1)), int16_t((y + end) / 2)}, 1, ink);
    }
    y = end;
    if (y >= r.bottom() - 1) break;
  }
}
inline void paint(toybox::Screen& screen, const Model& m, const Layout& layout, const pet::Profile* preview = nullptr,
                  int wallTop = -1) {
  const bool wide = layout.bounds.width > layout.bounds.height;
  const bool backgroundReady = roombackground::tiles(screen.target(), m, layout.bounds, wide);
  if (!backgroundReady) {
    backdrop(screen.target(), layout);
    window(m, screen.target(), layout.scene);
  }
  if (m.view != pet::View::Dashboard || preview) {
    static_assert(sizeof(Model) <= 256);
    Model backdrop = m;
    backdrop.view = m.view == pet::View::Evolution ? pet::View::Evolution : pet::View::Dashboard;
    if (preview) backdrop.profile = preview;
    habitatFigures(screen, backdrop, layout.scene, true, false, wallTop);
  } else {
    habitatFigures(screen, m, layout.scene, true, false, wallTop);
  }
}
}  // namespace petui::roomview
