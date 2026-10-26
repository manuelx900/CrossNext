#pragma once

#include <HalStorage.h>

#include <memory>

#include "ReadingStatsCore.h"

namespace readingstats::cloud {
enum class Result {
  Idle,
  Busy,
  Success,
  Empty,
  Cancelled,
  NoCredentials,
  AuthFailed,
  Unsupported,
  NetworkError,
  InvalidResponse,
  StorageError,
  LimitExceeded,
  LowMemory,
  NewerCache,
  LocalChanged
};
constexpr size_t kHeaderBytes = 3264, kDayBytes = 16, kBookBytes = 384;
constexpr uint32_t kMaxBooks = 20000, kMaxDays = 36600;
constexpr const char* kRoot = "/.crosspoint/reading-history";
constexpr const char* kResponse = "/.crosspoint/reading-history/sync-response.json";
struct Header {
  Summary summary;
  uint8_t active[92]{}, known[92]{};
  uint32_t generation = 0, books = 0, finished = 0, days = 0;
  int64_t importedAt = 0;
  uint64_t account = 0;
  uint64_t localIdentity = 0, localLength = 16, localHash = 14695981039346656037ULL;
};
void encodeHeader(const Header& header, uint8_t* bytes);
bool decodeHeader(const uint8_t* bytes, Header& header);
void encodeBook(const Book& book, uint8_t* bytes);
bool decodeBook(const uint8_t* bytes, Book& book);
const char* slotPath(int slot);

// Snapshot files are separate from the authoritative local session journal.
class Reader {
 public:
  Result open();
  void close();
  const Header& data() const { return header; }
  int activeSlot() const { return slot; }
  bool activeOn(int day) const;
  bool knownOn(int day) const;
  bool page(BookFilter filter, int page, Book* books, int capacity, int& count);
  bool find(const char* document, Book& book);
  bool project(PeriodStats& stats, bool* known = nullptr);
  bool at(uint32_t ordinal, Book& book) { return readBook(ordinal, book); }
  bool days(uint64_t* counters, size_t capacity);

 private:
  Header header;
  HalFile file;
  Book scratch;
  uint8_t bytes[kBookBytes]{};
  int slot = -1;
  bool validate(HalFile& candidate, uint8_t* headerBytes);
  bool readBook(uint32_t ordinal, Book& book);
};

using Fetch = Result (*)(void* context, const char* path, const char* destination);
class Projection;
class Importer {
 public:
  ~Importer();
  Result begin(int today, int64_t importedAt, uint64_t account, Fetch fetch, void* context,
               Projection* uploaded = nullptr);
  Result step();
  void cancel();
  uint32_t booksRead() const { return header.books; }
  Result status() const { return result; }
  void setDevice(const char* device);

 private:
  enum class Stage { Summary, Books, Commit };
  Projection* uploaded = nullptr;
  uint32_t ownBooks = 0;
  char ownDevice[96]{};
  Header header;
  Book pending, incoming, pendingOwn;
  HalFile completeBooks, unfinishedBooks, days;
  std::unique_ptr<uint8_t[]> bytes;
  Fetch fetch = nullptr;
  void* context = nullptr;
  Stage stage = Stage::Summary;
  Result result = Result::Idle;
  int slot = 0;
  uint32_t requests = 0;
  bool hasPending = false;
  bool writingSlot = false;
  char cursor[513]{}, lastDevice[129]{}, path[1664]{};
  Result fail(Result error);
  Result start(int today, int64_t importedAt, uint64_t account, Fetch source, void* user);
  void cleanup();
  bool flushBook();
  Result readSummary();
  Result readBooks();
  Result commit();
};
}  // namespace readingstats::cloud
