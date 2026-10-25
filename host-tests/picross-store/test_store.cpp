// Exercise the production persistence methods with FAT replacement semantics.
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../../src/apps_local/picross/PicrossSave.h"
#define LOG_ERR(...) ((void)0)
struct HalFile {
  std::vector<uint8_t>* bytes = nullptr;
  size_t position = 0;
  size_t size() const { return bytes ? bytes->size() : 0; }
  int read(void* out, size_t count) {
    if (!bytes) return 0;
    count = std::min(count, bytes->size() - position);
    std::memcpy(out, bytes->data() + position, count);
    position += count;
    return int(count);
  }
  size_t write(const void* value, size_t count) {
    if (!bytes) return 0;
    const auto* p = static_cast<const uint8_t*>(value);
    bytes->assign(p, p + count);
    return count;
  }
  bool seek(size_t p) {
    if (!bytes || p > bytes->size()) return false;
    position = p;
    return true;
  }
  void flush() {}
  void close() { bytes = nullptr; }
};
struct FatStorage {
  std::map<std::string, std::vector<uint8_t>> files;
  bool failAfterRemove = false;
  bool exists(const char* p) { return files.count(p); }
  bool openFileForRead(const char*, const char* p, HalFile& f) {
    auto it = files.find(p);
    if (it == files.end()) return false;
    f.bytes = &it->second;
    f.position = 0;
    return true;
  }
  bool openFileForWrite(const char*, const char* p, HalFile& f) {
    auto& bytes = files[p];
    bytes.clear();
    f.bytes = &bytes;
    f.position = 0;
    return true;
  }
  bool rename(const char* from, const char* to) {
    if (exists(to) || !exists(from)) return false;
    files[to] = std::move(files[from]);
    files.erase(from);
    return true;
  }
  bool replaceFile(const char* from, const char* to) {
    files.erase(to);
    if (failAfterRemove) return false;
    return rename(from, to);
  }
} Storage;
constexpr char kSavePath[] = "/.crosspoint/picross-v7.sav", kLegacyPath[] = "/.crosspoint/picross.sav";
constexpr int kSaveEvery = 8;
class PicrossActivity {
 public:
  picross::Board board;
  picross::Progress progress;
  uint8_t saveBytes[picrosssave::kBytes]{};
  int unsaved = 0;
  bool saveReadOnly = false;
  bool saveState();
  bool loadState();
  void touchSave();
  void flushSave();
};
#include "production.inc"
int main() {
  PicrossActivity a;
  a.board.load(0);
  a.progress.markSolved(0);
  a.unsaved = 1;
  a.flushSave();
  assert(a.unsaved == 0 && Storage.exists(kSavePath));
  a.board.load(449);
  a.progress.markSolved(449);
  a.unsaved = 1;
  a.flushSave();
  PicrossActivity b;
  assert(b.loadState() && b.board.index() == 449 && b.progress.isSolved(449));
  Storage.failAfterRemove = true;
  a.board.load(200);
  a.unsaved = 1;
  a.flushSave();
  assert(a.unsaved == 1 && !Storage.exists(kSavePath));
  PicrossActivity recovered;
  assert(recovered.loadState() && recovered.board.index() == 200 && recovered.progress.isSolved(449) &&
         recovered.unsaved == 1);
  Storage.failAfterRemove = false;
  recovered.flushSave();
  assert(recovered.unsaved == 0 && Storage.exists(kSavePath));
  auto future = Storage.files[kSavePath];
  future[0] = 8;
  Storage.files[kSavePath] = future;
  PicrossActivity readonly;
  assert(!readonly.loadState() && readonly.saveReadOnly);
  readonly.unsaved = 1;
  readonly.flushSave();
  assert(Storage.files[kSavePath] == future);
  Storage.files.clear();
  Storage.files["/.crosspoint/picross-v7.part"] = {7, 1};
  std::vector<uint8_t> old(132);
  old[0] = 6;
  old[1] = 5;
  old[104] = 1;
  Storage.files[kLegacyPath] = old;
  PicrossActivity legacy;
  assert(legacy.loadState() && legacy.board.index() == picross::kLegacyV6[5] &&
         legacy.progress.isSolved(picross::kLegacyV6[0]));
  std::puts(
      "Picross FAT: repeated replacement, interrupted publication recovery, retry, future-version protection, corrupt "
      "pending and legacy fallback passed");
}
