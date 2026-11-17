#pragma once

#include <algorithm>
#include <cstring>
#include <limits>

#include "ReadingStatsCore.h"

namespace readingstats {
// Borrow Recent for aggregation and Finished/detail for a ranked page plus look-ahead.
class PeriodBookPage {
 public:
  int first = 0, last = 0, page = 0, count = 0;
  bool hasNext = false;

  void begin(Book* slots, Book& scratch, Book* candidates, int capacity, int from, int through, int direction = 0) {
    anchor[0] = 0;
    anchorSeconds = 0;
    if (direction && count) {
      const auto& book = slots[direction > 0 ? count - 1 : 0];
      copyText(anchor, sizeof(anchor), book.document);
      anchorSeconds = book.seconds;
    }
    page = direction ? std::max(0, page + (direction > 0 ? 1 : -1)) : 0;
    first = from;
    last = through;
    books = slots;
    probe = &scratch;
    batch = candidates;
    limit = std::clamp(capacity, 1, kBooksPerPage);
    reverse = direction < 0;
    count = batchCount = 0;
    batchAnchor[0] = 0;
    batchMore = hasNext = false;
  }

  void add(const Record& record) {
    if (!record.seconds || record.kind != RecordKind::Session || !validDocument(record.book.document)) return;
    const int64_t start = int64_t(record.localDay) * 86400 + record.secondOfDay;
    const int64_t from = std::max(start, int64_t(first) * 86400);
    const int64_t to = std::min(start + record.seconds, (int64_t(last) + 1) * 86400);
    if (to <= from || (*batchAnchor && std::strcmp(record.book.document, batchAnchor) <= 0)) return;
    int at = 0;
    while (at < batchCount && std::strcmp(batch[at].document, record.book.document) < 0) ++at;
    if (at < batchCount && !std::strcmp(batch[at].document, record.book.document)) {
      auto& seconds = batch[at].seconds;
      seconds += std::min(uint64_t(to - from), std::numeric_limits<uint64_t>::max() - seconds);
      return;
    }
    if (batchCount == kRecentBooks) {
      batchMore = true;
      if (at == batchCount) return;
      --batchCount;
    }
    for (int n = batchCount; n > at; --n) batch[n] = batch[n - 1];
    batch[at] = record.book;
    batch[at].seconds = uint64_t(to - from);
    ++batchCount;
  }

  // Aggregate the inactive Recent cache's document totals per journal pass, then rank.
  bool finishScan() {
    if (batchCount) copyText(batchAnchor, sizeof(batchAnchor), batch[batchCount - 1].document);
    for (int n = 0; n < batchCount; ++n) {
      const auto& candidate = batch[n];
      if (candidate.seconds < kMinimumVisibleSeconds) continue;
      const int relative = compare(candidate.seconds, candidate.document, anchorSeconds, anchor);
      if (*anchor && (reverse ? relative >= 0 : relative <= 0)) continue;
      int at = 0;
      while (at < count) {
        const int order = compare(slot(at).seconds, slot(at).document, candidate.seconds, candidate.document);
        if (!(reverse ? order > 0 : order < 0)) break;
        ++at;
      }
      if (at > limit) continue;
      for (int i = std::min(count, limit); i > at; --i) slot(i) = slot(i - 1);
      slot(at) = candidate;
      count = std::min(count + 1, limit + 1);
    }
    batchCount = 0;
    if (batchMore) {
      batchMore = false;
      return true;
    }
    hasNext = reverse || count > limit;
    count = std::min(count, limit);
    finish();
    return false;
  }

 private:
  Book* books = nullptr;
  Book* probe = nullptr;
  Book* batch = nullptr;
  char anchor[33]{}, batchAnchor[33]{};
  uint64_t anchorSeconds = 0;
  int limit = kBooksPerPage, batchCount = 0;
  bool reverse = false, batchMore = false;
  Book& slot(int n) { return n < limit ? books[n] : *probe; }
  static int compare(uint64_t left, const char* leftKey, uint64_t right, const char* rightKey) {
    if (left != right) return left > right ? -1 : 1;
    return std::strcmp(leftKey, rightKey);
  }
  void finish() {
    if (reverse) {
      for (int n = 0; n < count / 2; ++n) {
        *probe = books[n];
        books[n] = books[count - n - 1];
        books[count - n - 1] = *probe;
      }
    }
  }
};
}  // namespace readingstats
