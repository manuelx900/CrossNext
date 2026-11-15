#pragma once

#include <HalMemory.h>
#include <Logging.h>
#include <components/lists/list-layout-cache.h>

// One optional PSRAM arena shared by the render task, independent of list size.
class UiListLayoutCache {
 public:
  static constexpr size_t CAPACITY = 96;
  static constexpr size_t BYTES = sizeof(freeink::ui::ListLayoutCache::Entry) * CAPACITY;
  static_assert(BYTES <= 4096);
  void prepare() {
    if (attempted) return;
    attempted = true;
    // Reused across screens; static internal storage would spend 3 KiB on an optional cache.
    storage = HalMemory::allocatePsram(BYTES);
    if (!storage) {
      LOG_ERR("LIST", "Optional row geometry cache unavailable: %u bytes", unsigned(BYTES));
      return;
    }
    cache.bind(reinterpret_cast<freeink::ui::ListLayoutCache::Entry*>(storage.get()), CAPACITY);
  }
  freeink::ui::ListLayoutCache* get() { return storage ? &cache : nullptr; }

 private:
  HalMemory::PsramBuffer storage;
  freeink::ui::ListLayoutCache cache;
  bool attempted = false;
};

inline UiListLayoutCache& sharedUiListLayoutCache() {
  static UiListLayoutCache cache;
  return cache;
}
