#include "FlashZoomUpgrade.h"

#include <FsHelpers.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

#include "FlashImport.h"
#include "FlashStorage.h"

namespace flashcards {
namespace {
struct UpgradeWork {
  Catalog catalog;
  std::unique_ptr<Importer> importer;
  HalFile original, rebuilt;
  char source[256]{}, expected[112]{}, scratch[112]{}, path[160]{}, target[160]{};
  uint8_t left[512]{}, right[512]{};
  bool ownsScratch = false;
  ~UpgradeWork() {
    importer.reset();
    original.close();
    rebuilt.close();
    if (!ownsScratch) return;
    for (const auto* name : {"deck.dat", "cards.dat", "meta.dat", "images.dat", "images.zoom.dat"}) {
      std::snprintf(path, sizeof(path), "%s/%s", scratch, name);
      if (Storage.exists(path) && !Storage.remove(path)) LOG_ERR("FLASH", "Cannot clean zoom scratch %s", path);
    }
    if (!Storage.rmdir(scratch)) LOG_ERR("FLASH", "Cannot remove zoom scratch directory");
  }
  bool run(const char* directory) {
    std::snprintf(path, sizeof(path), "%s/source.txt", directory);
    const size_t bytes = Storage.readFileToBuffer(path, source, sizeof(source) - 1);
    if (!bytes || bytes >= sizeof(source) - 1) return false;
    source[bytes] = 0;
    if (!FsHelpers::checkFileExtension(std::string_view(source), ".apkg") &&
        !FsHelpers::checkFileExtension(std::string_view(source), ".colpkg"))
      return false;
    // A changed package must not attach different media to existing review state.
    if (!catalog.resolve(source, expected, sizeof(expected)) || std::strcmp(expected, directory)) return false;
    const int length = std::snprintf(scratch, sizeof(scratch), "%s/.zoom-refresh", directory);
    if (length <= 0 || size_t(length) >= sizeof(scratch) || Storage.exists(scratch)) return false;
    ownsScratch = true;
    importer = makeUniqueNoThrow<Importer>();
    if (!importer) {
      LOG_ERR("FLASH", "OOM: zoom importer %u bytes", unsigned(sizeof(Importer)));
      return false;
    }
    if (!importer->begin(source, scratch, "Zoom")) return false;
    ImportResult result;
    do {
      result = importer->step();
      delay(1);
    } while (result == ImportResult::Busy);
    if (result != ImportResult::Complete) return false;
    importer.reset();
    std::snprintf(path, sizeof(path), "%s/deck.dat", directory);
    if (!Storage.openFileForRead("FLASH", path, original)) return false;
    std::snprintf(path, sizeof(path), "%s/deck.dat", scratch);
    if (!Storage.openFileForRead("FLASH", path, rebuilt) || original.fileSize64() != rebuilt.fileSize64()) return false;
    while (original.available()) {
      const int n = original.read(left, sizeof(left));
      if (n <= 0 || rebuilt.read(right, n) != n || std::memcmp(left, right, n)) return false;
      delay(1);
    }
    std::snprintf(path, sizeof(path), "%s/images.zoom.dat", scratch);
    std::snprintf(target, sizeof(target), "%s/images.zoom.dat", directory);
    return Storage.replaceFile(path, target);
  }
};
}  // namespace

bool upgradeZoomImages(const char* directory) {
  // Bounded importer + 1 KiB comparison workspace cannot live on the task stack.
  // Importer additionally owns its existing 524,320-byte PSRAM-only text workspace.
  auto work = makeUniqueNoThrow<UpgradeWork>();
  if (!work) {
    LOG_ERR("FLASH", "OOM: zoom upgrade %u bytes", unsigned(sizeof(UpgradeWork)));
    return false;
  }
  const bool ok = work->run(directory);
  if (!ok) LOG_ERR("FLASH", "Original package unavailable or changed; retaining image preview");
  return ok;
}
}  // namespace flashcards
