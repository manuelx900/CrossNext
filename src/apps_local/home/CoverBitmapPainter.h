#pragma once
#include <FreeInkUI.h>
class Bitmap;
class GfxRenderer;
namespace homecover {
// Shared render-owner workspace; center-crop to fill, or preserve contained artwork.
bool paintBitmap(const GfxRenderer& renderer, const Bitmap& bitmap, freeink::ui::Rect slot, bool fill = true,
                 int offset = 0, bool (*continueFn)(void*) = nullptr, void* context = nullptr,
                 bool preservePolarity = true);
}  // namespace homecover
