#include "VocabularyStorage.h"

#include <HalMemory.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "util/HtmlToPlainText.h"

namespace vocabulary {
// A backup can be the committed record after power loss. Do not enumerate it
// twice when the normal filename exists; the record reader validates both.
bool recordId(HalFile& file, const char* dir, char* name) {
  if (file.isDirectory()) return false;
  file.getName(name, 96);
  const auto n = std::strlen(name);
  if (n != 36 || (std::strcmp(name + 32, ".dat") && std::strcmp(name + 32, ".bak"))) return false;
  const bool backup = !std::strcmp(name + 32, ".bak");
  name[32] = 0;
  if (!vocabulary::validId(name)) return false;
  if (backup) {
    char path[96];
    std::snprintf(path, sizeof(path), "%s/%s.dat", dir, name);
    if (Storage.exists(path)) return false;
  }
  return true;
}
namespace {
void path(char* p, size_t n, const char* dir, const char* id, const char* suffix = ".dat") {
  std::snprintf(p, n, "%s/%s%s", dir, id, suffix);
}
bool erase(const char* p) { return !Storage.exists(p) || Storage.remove(p); }
bool validFile(const char* path) {
  HalFile file;
  if (!Storage.openFileForRead("WORDS", path, file) || file.size() < 20 || file.size() > kWireBytes) return false;
  const auto length = file.size() - 4;
  uint8_t chunk[48];
  size_t at = 0;
  uint32_t crc = 2166136261u;
  while (at < length) {
    const auto n = std::min(sizeof(chunk), size_t(length - at));
    if (file.read(chunk, n) != static_cast<int>(n)) return false;
    for (size_t i = 0; i < n; ++i) crc = (crc ^ chunk[i]) * 16777619u;
    at += n;
  }
  if (file.read(chunk, 4) != 4) return false;
  const uint32_t stored =
      uint32_t(chunk[0]) | uint32_t(chunk[1]) << 8 | uint32_t(chunk[2]) << 16 | uint32_t(chunk[3]) << 24;
  return crc == stored;
}
struct CaptureWork {
  StorageRecords files;
  Word incoming, old;
  Context oldContext;
};
static_assert(sizeof(CaptureWork) <= 16 * 1024, "Bound the cold lookup workspace");
struct UndoWork {
  StorageRecords files;
  Word word;
};
void copyMeaning(Word& word, const std::string& text) {
  size_t end = std::min(text.size(), sizeof(word.meaning) - 1);
  if (end < text.size())
    while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
  for (size_t i = 0; i < end; ++i) word.meaning[i] = text[i] ? text[i] : '\n';
  word.meaning[end] = 0;
  word.shortened = end < text.size();
}
}  // namespace
bool StorageRecords::ready() {
  return Storage.ensureDirectoryExists("/.crosspoint") && Storage.ensureDirectoryExists(kRoot) &&
         Storage.ensureDirectoryExists(kWords) && Storage.ensureDirectoryExists(kContexts);
}
template <typename T>
Read StorageRecords::read(const char* dir, const char* id, T& value) {
  if (!validId(id) || !Storage.ready()) return Read::Error;
  char name[96];
  bool any = false;
  for (const char* suffix : {".dat", ".bak"}) {
    path(name, sizeof(name), dir, id, suffix);
    if (!Storage.exists(name)) continue;
    any = true;
    HalFile file;
    if (!Storage.openFileForRead("WORDS", name, file)) continue;
    const auto n = file.size();
    if (n > sizeof(bytes) || n < 20 || file.read(bytes, n) != static_cast<int>(n)) continue;
    if (decode(bytes, n, value) && !std::strcmp(value.id, id)) return Read::Ok;
  }
  if (any) LOG_ERR("WORDS", "Unreadable record: %s/%s", dir, id);
  return any ? Read::Error : Read::Missing;
}
template <typename T>
bool StorageRecords::replace(const char* dir, const T& value) {
  if (!validId(value.id) || !ready()) return false;
  const auto n = encode(value, bytes, sizeof(bytes));
  if (!n) return false;
  // Keep the old record until the replacement has been closed and read back.
  // .bak is also a read fallback after loss of power between the two renames.
  char name[96];
  path(name, sizeof(name), dir, value.id, ".tmp");
  {
    HalFile file;
    if (!Storage.openFileForWrite("WORDS", name, file) || file.write(bytes, n) != n) return false;
    file.flush();
  }
  {
    HalFile file;
    if (!Storage.openFileForRead("WORDS", name, file) || file.size() != n) return false;
    uint8_t chunk[48];
    size_t at = 0;
    while (at < n) {
      const size_t amount = std::min(sizeof(chunk), n - at);
      if (file.read(chunk, amount) != static_cast<int>(amount) || std::memcmp(chunk, bytes + at, amount)) return false;
      at += amount;
    }
  }
  char live[96];
  path(live, sizeof(live), dir, value.id);
  // Reuse the scratch pathname after the verified temporary is closed.
  path(name, sizeof(name), dir, value.id, ".bak");
  if (Storage.exists(live)) {
    if (validFile(live)) {
      if (!erase(name) || !Storage.rename(live, name)) return false;
    } else if (!erase(live))
      return false;
  }
  // With only a backup present, keep it until the new live file exists.
  path(name, sizeof(name), dir, value.id, ".tmp");
  if (!Storage.rename(name, live)) {
    LOG_ERR("WORDS", "Commit failed: %s", live);
    return false;
  }
  path(name, sizeof(name), dir, value.id, ".bak");
  erase(name);
  return true;
}
Read StorageRecords::word(const char* id, Word& w) { return read(kWords, id, w); }
Read StorageRecords::context(const char* id, Context& c) { return read(kContexts, id, c); }
bool StorageRecords::put(const Word& w) { return replace(kWords, w); }
bool StorageRecords::put(const Context& c) { return replace(kContexts, c); }
bool StorageRecords::remove(const char* dir, const char* id) {
  if (!validId(id)) return false;
  char name[96];
  // Delete the recovery copy first; deleting live first could resurrect a note.
  for (const char* suffix : {".bak", ".tmp", ".dat"}) {
    path(name, sizeof(name), dir, id, suffix);
    if (!erase(name)) return false;
  }
  return true;
}
bool StorageRecords::removeWord(const char* id) { return remove(kWords, id); }
bool StorageRecords::removeContext(const char* id) { return remove(kContexts, id); }
CaptureNotice saveLookup(Context& source, const char* surface, const char* headword, const std::string& definition,
                         bool html, const char* dictionary, const char* language) {
  CaptureNotice notice;
  notice.attempted = true;
  // About 12 KiB, cold lookup only; the definition is already resident. Keeping
  // this on the task stack or as a permanent global would burden every reader.
  auto work = makeUniqueNoThrow<CaptureWork>();
  if (!work) {
    LOG_ERR("WORDS", "OOM: capture %u bytes", unsigned(sizeof(CaptureWork)));
    notice.status = Capture::Failed;
    return notice;
  }
  auto& w = work->incoming;
  if (!copy(w.surface, sizeof(w.surface), surface) || !copy(w.headword, sizeof(w.headword), headword) ||
      !copy(w.dictionary, sizeof(w.dictionary), dictionary) || !copy(w.language, sizeof(w.language), language))
    return notice;
  // HTML conversion is the existing dictionary formatter. Its temporary is
  // cold-path bounded by Dictionary::MAX_DEFINITION_BYTES and freed here.
  if (html) {
    const auto heap = HalMemory::getDefaultHeap();
    if (heap.largestBlockBytes < definition.size() + 1 || heap.freeBytes < definition.size() + 4096) {
      LOG_ERR("WORDS", "Insufficient heap for dictionary text conversion");
      notice.status = Capture::Failed;
      return notice;
    }
    const auto plain = htmlToPlainText(definition);
    copyMeaning(w, plain);
  } else {
    copyMeaning(w, definition);
  }
  notice.status = capture(work->files, w, source, work->old, work->oldContext, notice.token);
  if (notice.status == Capture::Failed) LOG_ERR("WORDS", "Vocabulary capture failed");
  return notice;
}
bool undoLookup(Undo& token) {
  auto work = makeUniqueNoThrow<UndoWork>();
  if (!work) {
    LOG_ERR("WORDS", "OOM: undo %u bytes", unsigned(sizeof(UndoWork)));
    return false;
  }
  const bool ok = undo(work->files, token, work->word);
  if (!ok) LOG_ERR("WORDS", "Vocabulary undo failed");
  return ok;
}
}  // namespace vocabulary
