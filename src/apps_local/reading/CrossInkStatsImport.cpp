#include "CrossInkStatsImport.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <ContentVisibility.h>
#include <KOReaderDocumentId.h>
#include <LibraryBuilder.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

#include "ReadingStatsStore.h"

namespace readingstats::crossink {
namespace {
constexpr const char* roots[] = {"/crossink-import/.crosspoint", "/.crosspoint"};
constexpr const char* versions[] = {"stats_v5.bin", "stats_v4.bin", "stats_v3.bin", "stats_v2.bin", "stats.bin"};
constexpr const char* suffixes[] = {"", ".tmp", ".bak"};
using cloud::Result;
}  // namespace
bool installed() {
  return Storage.exists(cloud::snapshotPath(cloud::SnapshotKind::CrossInk, 0)) ||
         Storage.exists(cloud::snapshotPath(cloud::SnapshotKind::CrossInk, 1));
}
bool Importer::loadGlobal() {
  for (const char* base : roots) {
    for (const char* suffix : suffixes) {
      path = std::string(base) + "/global_stats.bin" + suffix;
      if (!Storage.exists(path.c_str())) continue;
      HalFile file;
      if (!Storage.openFileForRead("CROSSINK", path, file)) continue;
      const auto bytes = file.fileSize64();
      if (bytes > 159 || file.read(raw, bytes) != int(bytes) || !decodeGlobal(raw, bytes, global)) continue;
      root = base;
      return true;
    }
  }
  return false;
}
bool Importer::loadBook(const std::string& cache, Book& book) {
  for (const char* name : versions) {
    for (const char* suffix : suffixes) {
      const std::string candidate = cache + "/" + name + suffix;
      if (!Storage.exists(candidate.c_str())) continue;
      HalFile file;
      if (!Storage.openFileForRead("CROSSINK", candidate, file)) continue;
      const auto bytes = file.fileSize64();
      if (bytes > 73 || file.read(raw, bytes) != int(bytes)) continue;
      if (decodeBook(raw, bytes, book)) return true;
    }
  }
  return false;
}
Result Importer::begin() {
  cancel();
  cursor = count = 0;
  header = {};
  if (!loadGlobal()) return Result::Empty;
  // The existing index supplies paths and metadata, without opening/parsing every EPUB.
  // Ask the user to refresh Library first when no valid index exists.
  if (!index.open(library::libraryIndexPath())) return Result::Unsupported;
  const auto old = previous.open(cloud::SnapshotKind::CrossInk);
  if (old != Result::Success && old != Result::Empty) return old;
  if (previous.data().generation == UINT32_MAX) return Result::LimitExceeded;
  slot = previous.activeSlot() == 0 ? 1 : 0;
  header.generation = previous.data().generation + 1;
  previous.close();
  const auto capacity = std::max<uint32_t>(1, index.bookCount());
  // At most CLIX_MAX_RECORDS book snapshots in PSRAM; no internal-RAM fallback.
  memory = HalMemory::allocatePsram(sizeof(Book) * capacity);
  if (!memory) {
    LOG_ERR("CROSSINK", "OOM: import book snapshots");
    return Result::LowMemory;
  }
  books = reinterpret_cast<Book*>(memory.get());
  path.reserve(768);
  metadata.reserve(512);
  stage = Stage::Books;
  return Result::Busy;
}
Result Importer::step() {
  if (stage == Stage::Done) return Result::Idle;
  if (stage == Stage::Commit) return commit();
  if (cursor >= index.bookCount()) {
    stage = Stage::Commit;
    return Result::Busy;
  }
  if (!index.readRecord(cursor++, entry) || !index.readPath(entry, path)) return Result::StorageError;
  const auto dot = path.find_last_of('.');
  const std::string_view ext = dot == std::string::npos ? std::string_view{} : std::string_view(path).substr(dot);
  const char* prefix = content::asciiEqual(ext, ".xtc") || content::asciiEqual(ext, ".xtch") ? "xtc"
                       : content::asciiEqual(ext, ".txt") || content::asciiEqual(ext, ".md") ? "txt"
                                                                                             : "epub";
  const auto hash = library::clixPathHash(path.data(), path.size());
  char folder[64];
  std::snprintf(folder, sizeof(folder), "/%s_%llu", prefix, static_cast<unsigned long long>(hash));
  bool found = loadBook(root + folder, pending);
  if (!found) {
    const auto legacy = std::hash<std::string>{}(path);
    std::snprintf(folder, sizeof(folder), "/%s_%llu", prefix, static_cast<unsigned long long>(legacy));
    found = loadBook(root + folder, pending);
  }
  if (!found) return Result::Busy;
  const auto id = KOReaderDocumentId::calculate(path, hashBuffer, sizeof(hashBuffer));
  if (!validDocument(id.c_str())) return Result::Busy;
  copyText(pending.document, sizeof(pending.document), id.c_str());
  if (!index.readTitle(entry, metadata)) return Result::StorageError;
  copyText(pending.title, sizeof(pending.title), metadata.c_str());
  if (!index.readAuthorField(entry, metadata)) return Result::StorageError;
  copyText(pending.author, sizeof(pending.author), metadata.c_str());
  books[count++] = pending;
  return Result::Busy;
}
Result Importer::commit() {
  std::sort(books, books + count, [](const Book& a, const Book& b) { return std::strcmp(a.document, b.document) < 0; });
  // Multiple paths may identify the same book; take a cumulative maximum, never a sum.
  uint32_t unique = 0;
  for (uint32_t n = 0; n < count; ++n) {
    if (unique && !std::strcmp(books[unique - 1].document, books[n].document)) {
      auto& prior = books[unique - 1];
      prior.seconds = std::max(prior.seconds, books[n].seconds);
      prior.pages = std::max(prior.pages, books[n].pages);
      prior.sessions = std::max(prior.sessions, books[n].sessions);
      prior.completed |= books[n].completed;
      continue;
    }
    books[unique++] = books[n];
  }
  count = unique;
  std::sort(books, books + count, [](const Book& a, const Book& b) {
    return a.completed != b.completed ? a.completed > b.completed : std::strcmp(a.document, b.document) < 0;
  });
  header.books = count;
  for (uint32_t n = 0; n < count; ++n) header.finished += books[n].completed;
  header.unattributedCompleted = global.completed > header.finished ? global.completed - header.finished : 0;
  header.summary.seconds = global.seconds;
  header.summary.pages = global.pages;
  header.summary.sessions = global.sessions;
  header.summary.completed = std::max(global.completed, header.finished);
  header.summary.anchorDay = global.anchor ? global.anchor : std::max(today(), kEpochDay);
  header.summary.longest = global.longest;
  std::memcpy(header.active, global.active, sizeof(header.active));
  if ((!Storage.exists(cloud::kRoot) && !Storage.mkdir(cloud::kRoot)) ||
      !Storage.openFileForWrite("CROSSINK", cloud::snapshotPath(cloud::SnapshotKind::CrossInk, slot), output))
    return Result::StorageError;
  writing = true;
  // Invalid header until every body record is durable. The other slot stays valid.
  std::memset(raw, 0, cloud::kHeaderBytes);
  if (output.write(raw, cloud::kHeaderBytes) != cloud::kHeaderBytes) return Result::StorageError;
  for (uint32_t n = 0; n < count; ++n) {
    cloud::encodeBook(books[n], raw);
    if (output.write(raw, cloud::kBookBytes) != cloud::kBookBytes) return Result::StorageError;
#ifdef ARDUINO
    if ((n & 31) == 0) delay(1);
#endif
  }
  cloud::encodeHeader(header, raw);
  if (!output.seek(0) || output.write(raw, cloud::kHeaderBytes) != cloud::kHeaderBytes) return Result::StorageError;
  output.flush();
  output.close();
  if (previous.open(cloud::SnapshotKind::CrossInk) != Result::Success || previous.activeSlot() != slot)
    return Result::StorageError;
  writing = false;
  stage = Stage::Done;
  index.close();
  previous.close();
  memory.reset();
  books = nullptr;
  return Result::Success;
}
void Importer::cancel() {
  output.close();
  previous.close();
  index.close();
  if (writing && slot >= 0) Storage.remove(cloud::snapshotPath(cloud::SnapshotKind::CrossInk, slot));
  writing = false;
  memory.reset();
  books = nullptr;
  stage = Stage::Done;
}
}  // namespace readingstats::crossink
