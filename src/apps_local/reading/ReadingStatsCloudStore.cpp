#include <BufferedFile.h>
#include <Logging.h>

#include "ReadingStatsCloud.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif
#include <Memory.h>

#include <algorithm>
#include <cstring>

namespace readingstats::cloud {
namespace {
uint64_t get(const uint8_t* p, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < n; ++i) v |= uint64_t(p[i]) << (i * 8);
  return v;
}
void put(uint8_t* p, uint64_t v, size_t n) {
  for (size_t i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (i * 8));
}
bool historyBit(const Header& h, const uint8_t* mask, int day) {
  const int n = h.summary.anchorDay - day;
  return n >= 0 && n < kHistoryDays && (mask[n / 8] & (1u << (n % 8)));
}
}  // namespace
const char* slotPath(int slot) {
  return slot == 0 ? "/.crosspoint/reading-history/sync-a.bin" : "/.crosspoint/reading-history/sync-b.bin";
}
void encodeHeader(const Header& h, uint8_t* p) {
  std::memset(p, 0, kHeaderBytes);
  std::memcpy(p, "CSI1", 4);
  put(p + 4, h.generation, 4);
  put(p + 8, h.books, 4);
  put(p + 12, h.finished, 4);
  put(p + 16, h.days, 4);
  put(p + 20, h.importedAt, 8);
  put(p + 28, h.account, 8);
  put(p + 36, h.summary.seconds, 8);
  put(p + 44, h.summary.pages, 8);
  put(p + 52, h.summary.sessions, 4);
  put(p + 56, h.summary.completed, 4);
  put(p + 60, h.summary.totalDays, 4);
  put(p + 64, h.summary.anchorDay, 4);
  put(p + 68, h.summary.lastReadDay, 4);
  put(p + 72, h.summary.run, 4);
  put(p + 76, h.summary.longest, 4);
  put(p + 80, h.localIdentity, 8);
  put(p + 88, h.localLength, 8);
  put(p + 96, h.localHash, 8);
  for (int n = 0; n < kHistoryDays; ++n) put(p + 128 + 4 * n, h.summary.dailySeconds[n], 4);
  std::memcpy(p + 3048, h.active, 92);
  std::memcpy(p + 3140, h.known, 92);
  put(p + kHeaderBytes - 4, checksum(p, kHeaderBytes - 4), 4);
}
bool decodeHeader(const uint8_t* p, Header& h) {
  if (std::memcmp(p, "CSI1", 4) || get(p + kHeaderBytes - 4, 4) != checksum(p, kHeaderBytes - 4)) return false;
  h = {};
  h.generation = get(p + 4, 4);
  h.books = get(p + 8, 4);
  h.finished = get(p + 12, 4);
  h.days = get(p + 16, 4);
  h.importedAt = get(p + 20, 8);
  h.account = get(p + 28, 8);
  h.summary.seconds = get(p + 36, 8);
  h.summary.pages = get(p + 44, 8);
  h.summary.sessions = get(p + 52, 4);
  h.summary.completed = get(p + 56, 4);
  h.summary.totalDays = get(p + 60, 4);
  h.summary.anchorDay = get(p + 64, 4);
  h.summary.lastReadDay = get(p + 68, 4);
  h.summary.run = get(p + 72, 4);
  h.summary.longest = get(p + 76, 4);
  h.localIdentity = get(p + 80, 8);
  h.localLength = get(p + 88, 8);
  h.localHash = get(p + 96, 8);
  if (!h.localLength) h.localLength = 16;
  for (int n = 0; n < kHistoryDays; ++n) h.summary.dailySeconds[n] = get(p + 128 + 4 * n, 4);
  std::memcpy(h.active, p + 3048, 92);
  std::memcpy(h.known, p + 3140, 92);
  return h.localLength >= 16 && (h.localLength - 16) % kRecordBytes == 0 && h.generation && h.books <= kMaxBooks &&
         h.finished <= h.books && h.days <= kMaxDays && h.summary.anchorDay >= dayIndex({2000, 1, 1}) &&
         h.summary.anchorDay <= dayIndex({2099, 12, 31});
}
void encodeBook(const Book& b, uint8_t* p) {
  std::memset(p, 0, kBookBytes);
  std::memcpy(p, "CSB1", 4);
  std::memcpy(p + 4, b.document, 33);
  std::memcpy(p + 37, b.title, 128);
  std::memcpy(p + 165, b.author, 64);
  put(p + 229, b.seconds, 8);
  put(p + 237, b.sessions, 4);
  put(p + 241, b.pages, 4);
  put(p + 245, b.firstEpoch, 8);
  put(p + 253, b.lastEpoch, 8);
  put(p + 261, b.finishedEpoch, 8);
  for (int n = 0; n < 4; ++n) put(p + 269 + n * 8, b.buckets[n], 8);
  put(p + 301, b.readingDays, 4);
  put(p + 305, b.lastDay, 4);
  put(p + 309, b.progressBp, 2);
  p[311] = b.completed;
  put(p + kBookBytes - 4, checksum(p, kBookBytes - 4), 4);
}
bool decodeBook(const uint8_t* p, Book& b) {
  if (std::memcmp(p, "CSB1", 4) || get(p + kBookBytes - 4, 4) != checksum(p, kBookBytes - 4) ||
      !std::memchr(p + 4, 0, 33) || !std::memchr(p + 37, 0, 128) || !std::memchr(p + 165, 0, 64) || p[311] > 1)
    return false;
  b = {};
  std::memcpy(b.document, p + 4, 33);
  std::memcpy(b.title, p + 37, 128);
  std::memcpy(b.author, p + 165, 64);
  b.seconds = get(p + 229, 8);
  b.sessions = get(p + 237, 4);
  b.pages = get(p + 241, 4);
  b.firstEpoch = get(p + 245, 8);
  b.lastEpoch = get(p + 253, 8);
  b.finishedEpoch = get(p + 261, 8);
  for (int n = 0; n < 4; ++n) b.buckets[n] = get(p + 269 + n * 8, 8);
  b.readingDays = get(p + 301, 4);
  b.lastDay = get(p + 305, 4);
  b.progressBp = get(p + 309, 2);
  b.completed = p[311];
  return validDocument(b.document) && b.progressBp <= 10000;
}
bool Reader::validate(HalFile& candidate, uint8_t* raw) {
  if (candidate.read(raw, kHeaderBytes) != kHeaderBytes || !decodeHeader(raw, header) ||
      candidate.fileSize64() != kHeaderBytes + uint64_t(header.days) * kDayBytes + uint64_t(header.books) * kBookBytes)
    return false;
  serialization::BufferedFileReader input(candidate, 3072);
  int previousDay = 0;
  for (uint32_t n = 0; n < header.days; ++n) {
    if (input.read(bytes, kDayBytes) != kDayBytes || get(bytes + 12, 4) != checksum(bytes, 12)) return false;
    const int day = get(bytes, 4);
    if (day <= previousDay || day < dayIndex({2000, 1, 1}) || day > dayIndex({2099, 12, 31})) return false;
    previousDay = day;
#ifdef ARDUINO
    if ((n & 255) == 0) delay(1);
#endif
  }
  char previousDocument[33]{};
  for (uint32_t n = 0; n < header.books; ++n) {
    if (input.read(bytes, kBookBytes) != kBookBytes || !decodeBook(bytes, scratch) ||
        scratch.completed != (n < header.finished))
      return false;
    if (n == header.finished) previousDocument[0] = 0;
    if (std::strcmp(previousDocument, scratch.document) >= 0) return false;
    std::strcpy(previousDocument, scratch.document);
#ifdef ARDUINO
    if ((n & 63) == 0) delay(1);
#endif
  }
  return true;
}
Result Reader::open() {
  close();
  // One bounded decode buffer, released after slot selection; no whole-file allocation.
  auto raw = makeUniqueNoThrow<uint8_t[]>(kHeaderBytes);
  if (!raw) {
    LOG_ERR("STATS-SYNC", "OOM: snapshot header");
    return Result::LowMemory;
  }
  uint32_t generation = 0;
  bool present = false, future = false;
  for (int n = 0; n < 2; ++n) {
    present |= Storage.exists(slotPath(n));
    HalFile candidate;
    if (!Storage.openFileForRead("STATS-SYNC", slotPath(n), candidate)) continue;
    if (candidate.read(raw.get(), 4) == 4 && !std::memcmp(raw.get(), "CSI", 3) && raw[3] > '1') future = true;
    if (candidate.seek(0) && validate(candidate, raw.get()) && header.generation > generation) {
      generation = header.generation;
      slot = n;
    }
  }
  if (future) {
    close();
    header = {};
    return Result::NewerCache;
  }
  if (slot < 0) {
    header = {};
    return present ? Result::StorageError : Result::Empty;
  }
  if (!Storage.openFileForRead("STATS-SYNC", slotPath(slot), file) ||
      file.read(raw.get(), kHeaderBytes) != kHeaderBytes || !decodeHeader(raw.get(), header)) {
    close();
    return Result::StorageError;
  }
  return Result::Success;
}
void Reader::close() {
  file.close();
  slot = -1;
}
bool Reader::activeOn(int day) const { return historyBit(header, header.active, day); }
bool Reader::knownOn(int day) const { return historyBit(header, header.known, day); }
bool Reader::readBook(uint32_t ordinal, Book& book) {
  return slot >= 0 && ordinal < header.books &&
         file.seek64(kHeaderBytes + uint64_t(header.days) * kDayBytes + uint64_t(ordinal) * kBookBytes) &&
         file.read(bytes, kBookBytes) == kBookBytes && decodeBook(bytes, book);
}
bool Reader::page(BookFilter filter, int page, Book* books, int capacity, int& count) {
  count = 0;
  if (slot < 0 || page < 0 || capacity <= 0 || !books) return false;
  const uint32_t total = filter == BookFilter::Finished ? header.finished : header.books - header.finished;
  const uint32_t base = filter == BookFilter::Finished ? 0 : header.finished;
  const uint64_t first = uint64_t(page) * capacity;
  for (uint64_t n = first; n < total && count < capacity; ++n) {
    if (!readBook(base + n, books[count])) return false;
    ++count;
  }
  return true;
}
bool Reader::find(const char* document, Book& book) {
  for (int group = 0; group < 2; ++group) {
    uint32_t lo = group ? header.finished : 0, hi = group ? header.books : header.finished;
    while (lo < hi) {
      const auto mid = lo + (hi - lo) / 2;
      if (!readBook(mid, scratch)) return false;
      const int comparison = std::strcmp(scratch.document, document);
      if (!comparison) {
        book = scratch;
        return true;
      }
      if (comparison < 0)
        lo = mid + 1;
      else
        hi = mid;
    }
  }
  return false;
}
bool Reader::days(uint64_t* counters, size_t capacity) {
  if (slot < 0 || !file.seek(kHeaderBytes)) return false;
  serialization::BufferedFileReader input(file, 2048);
  for (uint32_t n = 0; n < header.days; ++n) {
    if (input.read(bytes, kDayBytes) != kDayBytes || get(bytes + 12, 4) != checksum(bytes, 12)) return false;
    const int offset = int(get(bytes, 4)) - 10957;
    if (offset < 0 || size_t(offset) >= capacity) return false;
    counters[offset] = get(bytes + 4, 8);
  }
  return true;
}
bool Reader::project(PeriodStats& stats, bool* known) {
  if (known) *known = false;
  if (slot < 0 || !file.seek(kHeaderBytes)) return false;
  serialization::BufferedFileReader input(file, 2048);
  for (uint32_t n = 0; n < header.days; ++n) {
#ifdef ARDUINO
    if ((n & 255) == 0) delay(1);
#endif
    if (input.read(bytes, kDayBytes) != kDayBytes || get(bytes + 12, 4) != checksum(bytes, 12)) return false;
    const int day = get(bytes, 4);
    const uint64_t seconds = get(bytes + 4, 8);
    if (day < stats.first || day > stats.last) continue;
    if (known) *known = true;
    if (UINT64_MAX - stats.seconds < seconds) return false;
    stats.seconds += seconds;
    if (stats.annual) {
      const int month = dateFromDay(day).month - 1;
      stats.annualSeconds[month] += seconds;
      const int offset = day - stats.first;
      if (seconds) stats.annualDays[offset / 64] |= uint64_t(1) << (offset % 64);
    } else if (day - stats.first < 31)
      stats.dailySeconds[day - stats.first] = seconds;
  }
  return true;
}
}  // namespace readingstats::cloud
