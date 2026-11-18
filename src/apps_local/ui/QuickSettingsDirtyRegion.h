#pragma once

#include <cstring>

#include "QuickSettingsScreen.h"

namespace quickui {
inline fui::Rect dirtyRegion(const Model& before, const Model& after, const Layout& layout, int width) {
  fui::Rect dirty{};
  const auto add = [&](fui::Rect rect) {
    if (rect.empty()) return;
    if (dirty.empty()) {
      dirty = rect;
      return;
    }
    const int left = std::min<int>(dirty.x, rect.x), top = std::min<int>(dirty.y, rect.y);
    const int right = std::max<int>(dirty.right(), rect.right()), bottom = std::max<int>(dirty.bottom(), rect.bottom());
    dirty = fui::makeRect(left, top, right - left, bottom - top);
  };
  if (before.light != after.light || before.temperature != after.temperature || before.touch != after.touch ||
      before.night != after.night)
    return fui::makeRect(0, 0, width, layout.bottom);
  if (hasBluetoothStatus(before.bluetooth) != hasBluetoothStatus(after.bluetooth))
    return fui::makeRect(0, 0, width, layout.bottom);
  if (std::strcmp(before.time, after.time) || before.battery != after.battery || before.charging != after.charging ||
      before.bluetooth != after.bluetooth || before.wifiConnected != after.wifiConnected)
    add(layout.header);
  if (before.brightness != after.brightness || before.lightOn != after.lightOn) add(layout.brightness);
  if (before.warmth != after.warmth || before.lightOn != after.lightOn) add(layout.warmth);
  if (before.lightOn != after.lightOn || (before.warmth == 0) != (after.warmth == 0) ||
      before.bluetooth != after.bluetooth || before.wifiConnected != after.wifiConnected ||
      before.touchOn != after.touchOn || before.rotateRight != after.rotateRight ||
      std::memcmp(before.shortcuts, after.shortcuts, sizeof(before.shortcuts)))
    add(layout.tiles);
  // Unchanged model can still carry a pressed/released interaction style.
  return dirty.empty() ? fui::makeRect(0, 0, width, layout.bottom) : dirty;
}
}  // namespace quickui
