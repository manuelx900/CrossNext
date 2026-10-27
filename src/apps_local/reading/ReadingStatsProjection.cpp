#include "ReadingStatsProjection.h"

#include <BufferedFile.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "ReadingStatsStore.h"
#ifdef ARDUINO
#include <Arduino.h>
#endif
namespace readingstats::cloud {
namespace {
constexpr int firstDay = 10957;  // 2000-01-01, same epoch-day representation as the journal.
uint64_t hash(uint64_t value, const uint8_t* bytes, size_t size) {
  while (size--) value = (value ^ *bytes++) * 1099511628211ULL;
  return value;
}
void yieldIo(uint32_t n) {
#ifdef ARDUINO
  if ((n & 63) == 0) delay(1);
#else
  (void)n;
#endif
}
int nibble(char c) { return c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10; }
void document(const uint8_t* bytes, char* out) {
  static constexpr char hex[] = "0123456789abcdef";
  for (int n = 0; n < 16; ++n) {
    out[2 * n] = hex[bytes[n] >> 4];
    out[2 * n + 1] = hex[bytes[n] & 15];
  }
  out[32] = 0;
}
}  // namespace
void Projection::close() {
  journal.close();
  indexMemory.reset();
  orderMemory.reset();
  dayMemory.reset();
  entries = nullptr;
  order = nullptr;
  daily = nullptr;
  remote = nullptr;
  countBooks = 0;
  header = {};
  current = {};
}
Projection::Entry* Projection::locate(const char* text, bool create) {
  if (!validDocument(text)) return nullptr;
  uint8_t key[16];
  for (int n = 0; n < 16; ++n) key[n] = (nibble(text[2 * n]) << 4) | nibble(text[2 * n + 1]);
  uint32_t at = checksum(key, 16) & (kSlots - 1);
  for (uint32_t n = 0; n < kSlots; ++n, at = (at + 1) & (kSlots - 1)) {
    auto& e = entries[at];
    if (e.used) {
      if (!std::memcmp(e.document, key, 16)) return &e;
      continue;
    }
    if (!create || countBooks >= kMaxBooks) return nullptr;
    e.used = true;
    std::memcpy(e.document, key, 16);
    order[countBooks++] = at;
    return &e;
  }
  return nullptr;
}
bool Projection::read(uint64_t offset, Book& out) {
  if (!offset) {
    out = {};
    return true;
  }
  if (!journal.seek64(offset) || journal.read(raw, sizeof(raw)) != sizeof(raw) || !decode(raw, record)) return false;
  out = record.book;
  return true;
}
Result Projection::open(Reader* source, int today, uint64_t device, const Header* acknowledged) {
  close();
  remote = source;
  bool ready = false;
  ScopedCleanup cleanup([this, &ready] {
    if (!ready) close();
  });
  if (!device) return Result::StorageError;
  // 32768 fixed hash slots, 20000 ordinals and 100 years of day counters: <2 MiB PSRAM.
  // External memory has no internal-RAM fallback; SD book bodies are loaded on demand.
  indexMemory = HalMemory::allocatePsram(sizeof(Entry) * kSlots);
  orderMemory = HalMemory::allocatePsram(sizeof(uint32_t) * kMaxBooks);
  dayMemory = HalMemory::allocatePsram(sizeof(uint64_t) * kDates);
  if (!indexMemory || !orderMemory || !dayMemory) {
    LOG_ERR("STATS-SYNC", "OOM: projection PSRAM");
    close();
    return Result::LowMemory;
  }
  std::memset(indexMemory.get(), 0, sizeof(Entry) * kSlots);
  std::memset(dayMemory.get(), 0, sizeof(uint64_t) * kDates);
  entries = reinterpret_cast<Entry*>(indexMemory.get());
  order = reinterpret_cast<uint32_t*>(orderMemory.get());
  daily = reinterpret_cast<uint64_t*>(dayMemory.get());
  if (remote) header = remote->data();
  int anchor = today >= firstDay && today < firstDay + int(kDates) ? today : header.summary.anchorDay;
  if (anchor < firstDay || anchor >= firstDay + int(kDates)) anchor = firstDay;
  header.summary.anchorDay = anchor;
  uint8_t oldActive[92]{};
  if (remote)
    for (int n = 0; n < kHistoryDays; ++n)
      if (remote->activeOn(anchor - n)) oldActive[n / 8] |= 1u << (n % 8);
  std::memcpy(header.active, oldActive, 92);
  std::memset(header.known, 0, 92);
  if (remote)
    for (int n = 0; n < kHistoryDays; ++n)
      if (remote->knownOn(anchor - n)) header.known[n / 8] |= 1u << (n % 8);
  if (remote) {
    if (!remote->days(daily, kDates)) return Result::StorageError;
    for (uint32_t n = 0; n < remote->data().books; ++n) {
      if (!remote->at(n, base)) return Result::StorageError;
      auto* entry = locate(base.document, true);
      if (!entry) return Result::LimitExceeded;
      entry->cloud = n + 1;
      entry->finished = base.completed;
      yieldIo(n);
    }
  }
  cutoff = 16;
  if (Storage.exists(kJournalPath)) {
    if (!Storage.openFileForRead("STATS-SYNC", kJournalPath, journal) || journal.read(raw, 16) != 16 ||
        std::memcmp(raw, kJournalHeader, sizeof(kJournalHeader)))
      return Result::StorageError;
    const uint64_t rawSize = journal.fileSize64();
    if (rawSize < 16) return Result::StorageError;
    const uint64_t size = 16 + (rawSize - 16) / kRecordBytes * kRecordBytes;
    current.length = size;
    if (size > 16) {
      if (journal.read(raw, kRecordBytes) != kRecordBytes || !decode(raw, record)) return Result::StorageError;
      current.identity = hash(device, raw, kRecordBytes);
    }
    const auto& checkpoint = remote ? remote->data() : acknowledged ? *acknowledged : header;
    const bool same =
        (remote || acknowledged) && checkpoint.localIdentity && checkpoint.localIdentity == current.identity;
    if (same) {
      if (checkpoint.localLength > size) return Result::LocalChanged;
      if (remote) cutoff = checkpoint.localLength;
    }
    if (!journal.seek(16)) return Result::StorageError;
    serialization::BufferedFileReader input(journal, 3072);
    uint64_t fingerprint = 14695981039346656037ULL;
    for (uint64_t at = 16; at < size; at += kRecordBytes) {
      if (input.read(raw, kRecordBytes) != kRecordBytes || !decode(raw, record)) return Result::StorageError;
      fingerprint = hash(fingerprint, raw, kRecordBytes);
      if (same && at + kRecordBytes == checkpoint.localLength && fingerprint != checkpoint.localHash)
        return Result::LocalChanged;
      auto* entry = locate(record.book.document, true);
      if (!entry) return Result::LimitExceeded;
      entry->latest = at;
      if (at + kRecordBytes <= cutoff) entry->prior = at;
      if (at >= cutoff) {
        if (!remote && today < firstDay)
          anchor = std::max(
              anchor, record.localDay +
                          int((uint64_t(record.secondOfDay) + (record.seconds ? record.seconds - 1 : 0)) / 86400));
        auto& summary = header.summary;
        if (UINT64_MAX - summary.seconds < record.seconds || UINT64_MAX - summary.pages < record.pages ||
            (record.kind == RecordKind::Session && summary.sessions == UINT32_MAX))
          return Result::LimitExceeded;
        summary.seconds += record.seconds;
        summary.pages += record.pages;
        if (record.kind == RecordKind::Session) ++summary.sessions;
        if (record.firstCompletion && !entry->finished) {
          if (summary.completed == UINT32_MAX) return Result::LimitExceeded;
          ++summary.completed;
        }
        uint64_t left = record.seconds;
        uint32_t second = record.secondOfDay;
        int day = record.localDay;
        while (left) {
          if (day < firstDay || day >= firstDay + int(kDates)) return Result::InvalidResponse;
          const uint64_t take = std::min<uint64_t>(left, 21600 - second % 21600);
          if (UINT64_MAX - daily[day - firstDay] < take) return Result::LimitExceeded;
          daily[day - firstDay] += take;
          tod[((second / 21600) + 3) % 4] += take;
          dow[weekday(day)] += take;
          second += take;
          left -= take;
          if (second == 86400) {
            second = 0;
            ++day;
          }
        }
      }
      entry->finished = record.book.manuallyUnfinished ? false : entry->finished || record.book.completed;
      yieldIo((at - 16) / kRecordBytes);
    }
    current.hash = fingerprint;
  }
  std::sort(order, order + countBooks, [this](uint32_t a, uint32_t b) {
    const auto& x = entries[a];
    const auto& y = entries[b];
    return x.finished != y.finished ? x.finished > y.finished : std::memcmp(x.document, y.document, 16) < 0;
  });
  header.books = countBooks;
  header.finished = 0;
  for (uint32_t n = 0; n < countBooks; ++n) header.finished += entries[order[n]].finished;
  // Completion is a book state, including explicit local reversals, not a sum across devices.
  header.summary.completed = header.finished;
  header.summary.anchorDay = anchor;
  auto& s = header.summary;
  s.totalDays = 0;
  s.run = 0;
  s.lastReadDay = 0;
  std::memset(s.dailySeconds, 0, sizeof(s.dailySeconds));
  uint32_t run = 0;
  for (int n = 0; n < int(kDates); ++n) {
    const int day = firstDay + n, off = anchor - day;
    const bool in = off >= 0 && off < kHistoryDays;
    if (in && daily[n]) {
      if (daily[n] > UINT32_MAX) return Result::LimitExceeded;
      s.dailySeconds[off] = daily[n];
      header.active[off / 8] |= 1u << (off % 8);
      header.known[off / 8] |= 1u << (off % 8);
    }
    const bool active = daily[n] || (in && (header.active[off / 8] & (1u << (off % 8))));
    if (active) {
      ++s.totalDays;
      ++run;
      s.lastReadDay = day;
      s.run = run;
      s.longest = std::max(s.longest, run);
    } else
      run = 0;
  }
  ready = true;
  return Result::Success;
}
bool Projection::merge(const Entry& e, Book& out) {
  base = {};
  if (e.cloud && (!remote || !remote->at(e.cloud - 1, base))) return false;
  if (!read(e.latest, latest) || !read(e.prior, prior)) return false;
  if (latest.seconds < prior.seconds || latest.sessions < prior.sessions || latest.pages < prior.pages) return false;
  out = base;
  const uint64_t seconds = latest.seconds - prior.seconds;
  if (UINT64_MAX - out.seconds < seconds || UINT32_MAX - out.sessions < latest.sessions - prior.sessions ||
      UINT32_MAX - out.pages < latest.pages - prior.pages)
    return false;
  out.seconds += seconds;
  out.sessions += latest.sessions - prior.sessions;
  out.pages += latest.pages - prior.pages;
  for (int n = 0; n < 4; ++n) {
    if (latest.buckets[n] < prior.buckets[n] || UINT64_MAX - out.buckets[n] < latest.buckets[n] - prior.buckets[n])
      return false;
    out.buckets[n] += latest.buckets[n] - prior.buckets[n];
  }
  if (e.latest) {
    if (*latest.title) copyText(out.title, sizeof(out.title), latest.title);
    if (*latest.author) copyText(out.author, sizeof(out.author), latest.author);
    if (!out.firstEpoch || (latest.firstEpoch && latest.firstEpoch < out.firstEpoch))
      out.firstEpoch = latest.firstEpoch;
    out.lastEpoch = std::max(out.lastEpoch, latest.lastEpoch);
    out.finishedEpoch = std::max(out.finishedEpoch, latest.finishedEpoch);
    out.completed = latest.manuallyUnfinished ? false : out.completed || latest.completed;
    out.manuallyUnfinished = latest.manuallyUnfinished;
    if (latest.manuallyUnfinished) out.finishedEpoch = 0;
    if (e.latest >= cutoff) out.progressBp = latest.progressBp;
  }
  if (out.completed) out.progressBp = 10000;
  document(e.document, out.document);
  return true;
}
bool Projection::bookAt(uint32_t ordinal, Book& out) {
  return ordinal < countBooks && merge(entries[order[ordinal]], out);
}
bool Projection::find(const char* id, Book& out) {
  auto* e = locate(id, false);
  return e && merge(*e, out);
}
bool Projection::page(BookFilter filter, int page, Book* out, int capacity, int& count) {
  count = 0;
  if (page < 0 || capacity < 1 || !out) return false;
  uint32_t first = filter == BookFilter::Finished ? 0 : header.finished;
  const uint32_t end = filter == BookFilter::Finished ? header.finished : countBooks;
  const uint64_t at = uint64_t(first) + uint64_t(page) * capacity;
  for (uint64_t n = at; n < end && count < capacity; ++n)
    if (!bookAt(n, out[count++])) return false;
  return true;
}
uint64_t Projection::daySeconds(int day) const {
  return daily && day >= firstDay && day < firstDay + int(kDates) ? daily[day - firstDay] : 0;
}
bool Projection::project(PeriodStats& out, bool* known) const {
  if (known) *known = false;
  for (int day = std::max(firstDay, out.first); day <= std::min(firstDay + int(kDates) - 1, out.last); ++day) {
    const auto seconds = daySeconds(day);
    const int off = header.summary.anchorDay - day;
    if (known && (seconds || (off >= 0 && off < kHistoryDays && (header.known[off / 8] & (1u << (off % 8))))))
      *known = true;
    if (!seconds) continue;
    if (UINT64_MAX - out.seconds < seconds) return false;
    out.seconds += seconds;
    if (out.annual) {
      out.annualSeconds[dateFromDay(day).month - 1] += seconds;
      const int n = day - out.first;
      out.annualDays[n / 64] |= uint64_t(1) << (n % 64);
    } else if (day - out.first < 31)
      out.dailySeconds[day - out.first] = seconds;
  }
  return true;
}
}  // namespace readingstats::cloud
