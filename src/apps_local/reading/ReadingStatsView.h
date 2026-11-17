#pragma once

#include <HalStorage.h>

#include <cstring>

#include "ReadingStatsCore.h"

namespace readingstats {
// Disposable activity-owned SD rows. Work and I/O buffers belong to the existing statistics activity.
class PagedView {
 public:
  enum class Kind { Period, Days };
  enum class State { Closed, Collecting, Sorting, Folding, Ready, Failed };
  static constexpr size_t ROW_BYTES = 256, IO_BYTES = 3 * 1024;
  static_assert(kRecentBooks >= 10 && kRecordBytes * 8 >= IO_BYTES);
  bool begin(Kind kind, Book* work, uint8_t* io, int first, int last, const char* document = "");
  bool add(const Record& record);
  bool drain();
  bool pending() const { return pendingTo > pendingFrom; }
  bool finishInput();
  void step();
  bool readPage(int page, int capacity, Book* out, int& count);
  void close();
  State state() const { return phase; }
  int count() const { return int(rows); }
  bool matches(Kind value, int from, int through, const char* selected = "") const {
    return phase == State::Ready && kind == value && first == from && last == through &&
           std::strcmp(document, selected) == 0;
  }

 private:
  HalFile input, output;
  Book* work = nullptr;
  uint8_t* io = nullptr;
  Kind kind = Kind::Period;
  State phase = State::Closed;
  char document[33]{};
  int first = 0, last = 0;
  int64_t pendingFrom = 0, pendingTo = 0;
  uint32_t rows = 0, written = 0, run = 8, pair = 0, left = 0, right = 0, leftEnd = 0, rightEnd = 0;
  uint32_t loadedAt[2]{}, loadedCount[2]{};
  uint8_t collected = 0, buffered = 0;
  bool sourceB = false, ranked = false, head[2]{}, group = false;
  const char* path(bool second) const;
  bool fail(const char* reason);
  bool flushCollected();
  bool startPass(State next);
  bool finishPass();
  bool append(const Book& book);
  bool flush();
  bool read(uint32_t row, Book& book, unsigned side);
  int compare(const Book& a, const Book& b) const;
  bool sameKey(const Book& a, const Book& b) const;
  static void encode(const Book& book, uint8_t* bytes);
  static bool decode(const uint8_t* bytes, Book& book);
};
}  // namespace readingstats
