#include "HomeMenuStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

namespace homemenu {
namespace {
constexpr const char* kPaths[] = {"/.crosspoint/home-menu-a.bin", "/.crosspoint/home-menu-b.bin"};
}
Store& Store::instance() {
  static Store store;
  return store;
}
const Config& config() {
  auto& store = Store::instance();
  store.load();
  return store.get();
}
bool Store::read(int index) {
  HalFile file;
  if (!Storage.openFileForRead("HOME-MENU", kPaths[index], file)) return false;
  if (file.read(buffer, 4) == 4 && !std::memcmp(buffer, "HMC", 3) && buffer[3] > kSaveVersion) readOnly = true;
  readLength = file.size();
  return (readLength == kSaveBytes || readLength == kLegacySaveBytes) && file.seek(0) &&
         file.read(buffer, readLength) == static_cast<int>(readLength);
}
void Store::load() {
  if (loaded) return;
  loaded = true;
  bool existed = false;
  for (int n = 0; n < 2; ++n) {
    existed |= Storage.exists(kPaths[n]);
    if (!read(n)) continue;
    if (!std::memcmp(buffer, "HMC", 3) && buffer[3] > kSaveVersion) {
      readOnly = true;
      continue;
    }
    uint32_t found = 0;
    if (decode(buffer, scratch, found, readLength) &&
        (slot < 0 || (found != generation && found - generation < 0x80000000u))) {
      current = scratch;
      generation = found;
      slot = n;
    }
  }
  if (existed && slot < 0) readOnly = true;
}
bool Store::save(const Config& next) {
  load();
  if (current.visible == next.visible && !std::memcmp(current.labels, next.labels, sizeof(current.labels)) &&
      !std::memcmp(current.shortcuts, next.shortcuts, sizeof(current.shortcuts)) &&
      !std::memcmp(current.order, next.order, sizeof(current.order)))
    return true;
  if (readOnly || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  encode(next, generation + 1, buffer);
  uint32_t found = 0;
  if (!decode(buffer, scratch, found)) return false;
  const int target = slot == 0 ? 1 : 0;
  {
    HalFile file;
    if (!Storage.openFileForWrite("HOME-MENU", kPaths[target], file) ||
        file.write(buffer, sizeof(buffer)) != sizeof(buffer)) {
      LOG_ERR("HOME-MENU", "Cannot save menu preferences");
      return false;
    }
  }
  if (!read(target) || !decode(buffer, scratch, found) || found != generation + 1) {
    LOG_ERR("HOME-MENU", "Menu preferences verification failed");
    return false;
  }
  current = scratch;
  generation = found;
  slot = target;
  return true;
}
}  // namespace homemenu
