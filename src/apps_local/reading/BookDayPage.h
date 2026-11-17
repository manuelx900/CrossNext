#pragma once

#include <algorithm>
#include <cstring>
#include <limits>

#include "ReadingStatsCore.h"

namespace readingstats {
class BookDayPage {
 public:
  struct Entry {
    uint64_t seconds = 0;
    int day = 0;
  };
  Entry entries[kBooksPerPage]{};
  int page = 0, count = 0, capacity = kBooksPerPage;
  int total = 0;
  bool hasNext = false;

  void begin(const char* selected, int slots, int direction = 0) {
    anchored = direction && count;
    anchor = anchored ? entries[direction > 0 ? count - 1 : 0].day : 0;
    page = direction ? std::max(0, page + (direction > 0 ? 1 : -1)) : 0;
    document = selected;
    capacity = std::clamp(slots, 1, kBooksPerPage);
    reverse = direction < 0;
    count = filled = rawCount = 0;
    rawMore = false;
    hasNext = reverse;
  }

  void add(const Record& record) {
    if (!document || record.kind != RecordKind::Session || !record.seconds ||
        std::strcmp(document, record.book.document))
      return;
    const int64_t start = int64_t(record.localDay) * 86400 + record.secondOfDay;
    const int64_t finish = start + record.seconds;
    int first = record.localDay, last = int((finish - 1) / 86400);
    if (anchored) {
      if (reverse)
        first = std::max(first, anchor + 1);
      else
        last = std::min(last, anchor - 1);
    }
    // Only the nearest capacity+1 dates can enter this page or establish a next page.
    for (int n = 0, day = reverse ? first : last; n <= capacity && day >= first && day <= last;
         ++n, day += reverse ? 1 : -1) {
      const int64_t from = std::max(start, int64_t(day) * 86400);
      const int64_t to = std::min(finish, (int64_t(day) + 1) * 86400);
      if (to > from) addDay(day, uint64_t(to - from));
    }
    if (int64_t(last) - first + 1 > capacity + 1) rawMore = true;
  }

  bool finishScan() {
    if (rawCount) anchor = slot(filled + rawCount - 1).day;
    anchored = anchored || rawCount;
    const int start = filled;
    for (int n = 0; n < rawCount; ++n) {
      if (slot(start + n).seconds < kMinimumVisibleSeconds) continue;
      if (filled == capacity) {
        hasNext = true;
        finish();
        return false;
      }
      if (filled != start + n) slot(filled) = slot(start + n);
      ++filled;
    }
    rawCount = 0;
    if (rawMore) {
      rawMore = false;
      return true;
    }
    finish();
    return false;
  }

 private:
  const char* document = nullptr;
  int anchor = 0, filled = 0, rawCount = 0;
  bool anchored = false, reverse = false, rawMore = false;
  Entry probe;
  Entry& slot(int n) { return n < capacity ? entries[n] : probe; }
  void finish() {
    count = filled;
    if (reverse) {
      for (int n = 0; n < count / 2; ++n) std::swap(entries[n], entries[count - n - 1]);
    }
  }
  void addDay(int day, uint64_t seconds) {
    int at = 0;
    while (at < rawCount && (reverse ? slot(filled + at).day < day : slot(filled + at).day > day)) ++at;
    if (at < rawCount && slot(filled + at).day == day) {
      auto& total = slot(filled + at).seconds;
      total += std::min(seconds, std::numeric_limits<uint64_t>::max() - total);
      return;
    }
    const int available = capacity + 1 - filled;
    if (rawCount == available) {
      rawMore = true;
      if (at == rawCount) return;
      --rawCount;
    }
    for (int n = rawCount; n > at; --n) slot(filled + n) = slot(filled + n - 1);
    slot(filled + at) = {seconds, day};
    ++rawCount;
  }
};
}  // namespace readingstats
