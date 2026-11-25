#pragma once

#include <FreeInkUI.h>

#include <cstdint>

class GfxRenderer;
class HalFile;

namespace homecover {
enum class SceneKind : uint8_t { Home, Library };

// Three bounded art-region snapshots; UI text and status are always redrawn.
bool restoreScene(GfxRenderer& renderer, freeink::ui::Rect region, uint64_t key, SceneKind kind = SceneKind::Home,
                  bool allowPack = false);
// Query only: leaves pixels, LRU age and a pending capture untouched.
bool containsScene(freeink::ui::Rect region, uint64_t key, SceneKind kind = SceneKind::Home);
bool prefetchLibraryScene(GfxRenderer& renderer, freeink::ui::Rect region, uint64_t key, bool (*continueWork)(void*),
                          void* context);
// File-transfer/cache mutations can preserve FAT timestamps and file sizes.
void invalidateScenes();
uint32_t sceneRevision();
void recordSource(bool resolved);
void storeScene(GfxRenderer& renderer, bool completeArtwork = false);
bool scenePackPending();
void stepScenePack();
void cancelScenePack();
}  // namespace homecover
