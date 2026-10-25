#pragma once

#include <FreeInkUI.h>

#include <cstdint>

class GfxRenderer;
class HalFile;

namespace homecover {
// Three bounded art-region snapshots; UI text and status are always redrawn.
bool restoreScene(GfxRenderer& renderer, freeink::ui::Rect region, uint64_t key);
// File-transfer/cache mutations can preserve FAT timestamps and file sizes.
void invalidateScenes();
void recordSource(const char* path, HalFile& file, bool valid);
void storeScene(GfxRenderer& renderer);
}  // namespace homecover
