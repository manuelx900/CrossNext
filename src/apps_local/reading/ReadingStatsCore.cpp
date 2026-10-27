#include "ReadingStatsCore.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace readingstats {
uint8_t formatMask(const char* path) {
  if (!path) return 0;
  const char* ext = std::strrchr(path, '.');
  if (!ext) return 0;
  char lower[8]{};
  for (int n = 0; ext[n] && n < 7; ++n) lower[n] = ext[n] >= 'A' && ext[n] <= 'Z' ? ext[n] + 32 : ext[n];
  if (!std::strcmp(lower, ".epub")) return 1;
  if (!std::strcmp(lower, ".txt")) return 2;
  if (!std::strcmp(lower, ".md")) return 4;
  if (!std::strcmp(lower, ".pdf")) return 8;
  if (!std::strcmp(lower, ".xtc") || !std::strcmp(lower, ".xtch")) return 16;
  if (!std::strcmp(lower, ".bmp") || !std::strcmp(lower, ".png") || !std::strcmp(lower, ".jpg") ||
      !std::strcmp(lower, ".jpeg") || !std::strcmp(lower, ".gif"))
    return 32;
  return 0;
}
bool enabledFormat(const char* path, uint8_t mask) { return (formatMask(path) & mask) != 0; }

namespace {
uint64_t read(const uint8_t* p, int n) {
  uint64_t v = 0;
  for (int i = 0; i < n; ++i) v |= uint64_t(p[i]) << (i * 8);
  return v;
}
void write(uint8_t* p, uint64_t v, int n) {
  for (int i = 0; i < n; ++i) p[i] = static_cast<uint8_t>(v >> (i * 8));
}
uint64_t sum(uint64_t a, uint64_t b) { return b > UINT64_MAX - a ? UINT64_MAX : a + b; }
uint32_t sum32(uint32_t a, uint32_t b) { return b > UINT32_MAX - a ? UINT32_MAX : a + b; }
}  // namespace
int daysInMonth(int year, int month) {
  static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) return 0;
  return days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
int dayIndex(Date d) {
  if (d.year < 1970 || d.year > 2199 || d.month < 1 || d.month > 12 || d.day < 1 ||
      d.day > daysInMonth(d.year, d.month))
    return 0;
  int y = d.year - (d.month <= 2);
  const int era = y / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (d.month + (d.month > 2 ? -3 : 9)) + 2) / 5 + d.day - 1;
  return era * 146097 + static_cast<int>(yoe * 365 + yoe / 4 - yoe / 100 + doy) - 719468;
}
Date dateFromDay(int day) {
  if (day < 0 || day > 84005) return {};
  int z = day + 719468;
  const int era = z / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int y = static_cast<int>(yoe) + era * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
  const int m = static_cast<int>(mp) + (mp < 10 ? 3 : -9);
  return {y + (m <= 2), m, d};
}
int weekday(int day) { return (day + 3) % 7; }
int weekStart(int day, bool sunday) { return day - (weekday(day) + (sunday ? 1 : 0)) % 7; }
int isoWeek(int day) {
  const int thursday = day + 3 - weekday(day);
  const auto year = dateFromDay(thursday).year;
  return (thursday - dayIndex({year, 1, 1})) / 7 + 1;
}
void setPeriod(PeriodStats& stats, Period period, int day, bool sunday) {
  stats = {};
  stats.first = stats.last = day;
  switch (period) {
    case Period::Calendar:
    case Period::Day:
      break;
    case Period::Week:
      stats.first = weekStart(day, sunday);
      stats.last = stats.first + 6;
      break;
    case Period::Year: {
      const auto d = dateFromDay(day);
      stats.first = dayIndex({d.year, 1, 1});
      stats.last = dayIndex({d.year + 1, 1, 1}) - 1;
      stats.annual = true;
      break;
    }
    case Period::Month: {
      const auto d = dateFromDay(day);
      stats.first = dayIndex({d.year, d.month, 1});
      stats.last = stats.first + daysInMonth(d.year, d.month) - 1;
      break;
    }
  }
}
void Streak::begin(int through) { first = minimumDay = (last = through) + 1; }
void Streak::add(const Record& r) {
  if (!last || !r.seconds || r.localDay > last) return;
  const int end = std::min(last, r.localDay + static_cast<int>((uint64_t(r.secondOfDay) + r.seconds - 1) / 86400));
  minimumDay = std::min(minimumDay, r.localDay);
  if (end >= first - 1) first = std::min(first, r.localDay);
}
uint32_t Streak::days() const { return last && first <= last ? static_cast<uint32_t>(last - first + 1) : 0; }
void PeriodStats::add(const Record& r) {
  consistency.add(r);
  if (last < first) return;
  const int64_t start = int64_t(r.localDay) * 86400 + r.secondOfDay;
  const int64_t from = std::max(start, int64_t(first) * 86400);
  const int64_t to = std::min(start + r.seconds, int64_t(last + 1) * 86400);
  const bool reverse = r.kind == RecordKind::Reopen;
  const int64_t completedAt = reverse ? int64_t(r.reversedCompletionDay) * 86400 : start + r.seconds;
  if ((r.firstCompletion || reverse) && completedAt >= int64_t(first) * 86400 &&
      completedAt < int64_t(last + 1) * 86400) {
    // Period scans run backwards. Keep unmatched reversals until their original completion is encountered.
    if (reverse) {
      if (completed)
        --completed;
      else
        ++pendingReversals;
    } else if (pendingReversals)
      --pendingReversals;
    else
      completed = sum32(completed, 1);
    if (annual) {
      const int month = dateFromDay(static_cast<int>(completedAt / 86400)).month - 1;
      if (reverse) {
        if (annualCompleted[month])
          --annualCompleted[month];
        else
          ++annualReversals[month];
      } else if (annualReversals[month])
        --annualReversals[month];
      else
        annualCompleted[month] = sum32(annualCompleted[month], 1);
    }
  }
  if (!r.seconds || to <= from) return;
  seconds = sum(seconds, static_cast<uint64_t>(to - from));
  // Cumulative rounding assigns every page once across adjoining periods.
  pages = sum(pages, uint64_t(r.pages) * (to - start) / r.seconds - uint64_t(r.pages) * (from - start) / r.seconds);
  sessions = sum32(sessions, 1);
  const int beginDay = std::max(0, int(from / 86400) - first);
  const int endDay = std::min(last - first, int((to - 1) / 86400) - first);
  for (int n = beginDay; n <= endDay; ++n) {
    const int64_t day = int64_t(first + n) * 86400;
    const int64_t length = std::min(to, day + 86400) - std::max(from, day);
    if (length > 0) {
      if (annual && n < 366) {
        const int month = dateFromDay(first + n).month - 1;
        annualSeconds[month] = sum(annualSeconds[month], static_cast<uint64_t>(length));
        annualDays[n / 64] |= uint64_t{1} << (n % 64);
      } else if (n < 31)
        dailySeconds[n] = sum(dailySeconds[n], static_cast<uint64_t>(length));
    }
  }
  // Six time-of-day bins are used by the day view only.
  if (first != last) return;
  for (int n = 0; n < 6; ++n) {
    const int64_t bin = int64_t(first) * 86400 + n * 14400;
    const int64_t length = std::min(to, bin + 14400) - std::max(from, bin);
    if (length > 0) hours[n] = sum(hours[n], static_cast<uint64_t>(length));
  }
}
int PeriodStats::daysReadThrough(int day) const {
  int count = 0;
  for (int n = 0; n <= std::min(annual ? 365 : 30, std::min(day, last) - first); ++n)
    count += annual ? ((annualDays[n / 64] >> (n % 64)) & 1) != 0 : dailySeconds[n] > 0;
  return count;
}
void addRecent(Book* books, int& count, const Book& book) {
  int at = 0;
  while (at < count && std::strcmp(books[at].document, book.document)) ++at;
  if (book.completed) {
    if (at < count) {
      for (int n = at + 1; n < count; ++n) books[n - 1] = books[n];
      --count;
    }
    return;
  }
  if (at == count) count = std::min(kRecentBooks, count + 1);
  for (int n = std::min(at, kRecentBooks - 1); n > 0; --n) books[n] = books[n - 1];
  books[0] = book;
}
bool validDocument(const char* id) {
  if (!id || std::strlen(id) != 32) return false;
  for (int n = 0; n < 32; ++n)
    if (!((id[n] >= '0' && id[n] <= '9') || (id[n] >= 'a' && id[n] <= 'f'))) return false;
  return true;
}
void copyText(char* out, size_t size, const char* text) {
  if (!size) return;
  if (!text) text = "";
  size_t n = std::min(size - 1, std::strlen(text));
  while (n && (static_cast<unsigned char>(text[n]) & 0xc0) == 0x80) --n;
  std::memset(out, 0, size);
  std::memcpy(out, text, n);
}
void updateBook(Record& record, const Book& previous) {
  auto& b = record.book;
  b.seconds = sum(previous.seconds, record.seconds);
  b.sessions = sum32(previous.sessions, record.kind == RecordKind::Session ? 1 : 0);
  b.pages = sum32(previous.pages, record.pages);
  b.firstEpoch = previous.firstEpoch > 0 || !record.seconds ? previous.firstEpoch : record.epoch;
  b.lastEpoch = record.kind == RecordKind::Reopen ? previous.lastEpoch : record.epoch + record.seconds;
  b.manuallyUnfinished =
      record.kind == RecordKind::Reopen || (previous.manuallyUnfinished && record.kind != RecordKind::Completion);
  b.completed = !b.manuallyUnfinished && (previous.completed || b.progressBp >= 10000);
  if (b.manuallyUnfinished)
    b.progressBp = std::min<uint16_t>(9999, b.progressBp);
  else if (b.completed)
    b.progressBp = 10000;
  record.firstCompletion = b.completed && !previous.completed;
  b.finishedEpoch = !b.completed ? 0 : previous.completed ? previous.finishedEpoch : b.lastEpoch;
  b.finishedDay = !b.completed ? 0
                  : previous.completed
                      ? previous.finishedDay
                      : record.localDay + static_cast<int>((uint64_t(record.secondOfDay) + record.seconds) / 86400);
  if (record.kind == RecordKind::Reopen) record.reversedCompletionDay = previous.completed ? previous.finishedDay : 0;
  b.readingDays = previous.readingDays;
  b.lastDay = previous.lastDay;
  for (int n = 0; n < 4; ++n) b.buckets[n] = previous.buckets[n];
  if (!record.seconds) return;
  const int endDay = record.localDay + static_cast<int>((uint64_t(record.secondOfDay) + record.seconds - 1) / 86400);
  if (endDay > previous.lastDay) {
    b.readingDays =
        sum32(b.readingDays, static_cast<uint32_t>(endDay - std::max(record.localDay - 1, previous.lastDay)));
    b.lastDay = endDay;
  }
  uint64_t left = record.seconds;
  uint32_t at = record.secondOfDay;
  const auto wholeDays = left / 86400;
  for (int n = 0; n < 4; ++n) b.buckets[n] = sum(b.buckets[n], wholeDays * 21600);
  left %= 86400;
  while (left) {
    const int hour = at / 3600;
    const int bucket = hour / 6;
    const uint32_t boundary = hour < 6 ? 6 * 3600 : hour < 12 ? 12 * 3600 : hour < 18 ? 18 * 3600 : 24 * 3600;
    const auto take = std::min<uint64_t>(left, boundary - at);
    b.buckets[bucket] = sum(b.buckets[bucket], take);
    left -= take;
    at = boundary % 86400;
  }
}
void Summary::add(const Record& r) {
  seconds = sum(seconds, r.seconds);
  pages = sum(pages, r.pages);
  sessions = sum32(sessions, r.kind == RecordKind::Session ? 1 : 0);
  if (r.kind == RecordKind::Reopen && r.reversedCompletionDay && completed)
    --completed;
  else
    completed = sum32(completed, r.firstCompletion ? 1 : 0);
  if (!r.seconds) return;
  const int end = r.localDay + static_cast<int>((uint64_t(r.secondOfDay) + r.seconds - 1) / 86400);
  const int first = std::max(r.localDay, anchorDay - kHistoryDays + 1);
  const int last = std::min(end, anchorDay);
  for (int day = first; day <= last; ++day) {
    const int64_t start = int64_t(day - r.localDay) * 86400 - r.secondOfDay;
    const auto from = std::max<int64_t>(0, start);
    const auto to = std::min<int64_t>(r.seconds, start + 86400);
    if (to > from)
      dailySeconds[anchorDay - day] =
          std::min<uint32_t>(86400, sum32(dailySeconds[anchorDay - day], static_cast<uint32_t>(to - from)));
  }
  if (end > lastReadDay) {
    totalDays = sum32(totalDays, static_cast<uint32_t>(end - std::max(r.localDay - 1, lastReadDay)));
    if (!lastReadDay || r.localDay > lastReadDay + 1)
      run = static_cast<uint32_t>(end - r.localDay + 1);
    else
      run = sum32(run, static_cast<uint32_t>(end - lastReadDay));
    lastReadDay = end;
    longest = std::max(longest, run);
  }
}
uint32_t Summary::onDay(int day) const {
  const int n = anchorDay - day;
  return n >= 0 && n < kHistoryDays ? dailySeconds[n] : 0;
}
uint32_t Summary::currentStreak(int today) const {
  if (lastReadDay == today || lastReadDay == today - 1) return run;
  return 0;
}
uint32_t Summary::daysRead() const { return totalDays; }
uint32_t checksum(const uint8_t* bytes, size_t length) {
  uint32_t v = 2166136261u;
  while (length--) v = (v ^ *bytes++) * 16777619u;
  return v;
}
void encode(const Record& r, uint8_t* p) {
  std::memset(p, 0, kRecordBytes);
  std::memcpy(p, "RSE1", 4);
  write(p + 4, r.seconds, 4);
  write(p + 8, r.pages, 4);
  p[12] = r.firstCompletion;
  p[13] = static_cast<uint8_t>(r.kind);
  write(p + 14, r.book.progressBp, 2);
  write(p + 16, r.epoch, 8);
  write(p + 24, r.book.seconds, 8);
  write(p + 32, r.book.sessions, 4);
  write(p + 36, r.book.pages, 4);
  write(p + 40, r.book.firstEpoch, 8);
  write(p + 48, r.book.lastEpoch, 8);
  write(p + 56, r.book.finishedEpoch, 8);
  write(p + 64, r.book.readingDays, 4);
  write(p + 68, r.book.lastDay, 4);
  for (int n = 0; n < 4; ++n) write(p + 72 + 8 * n, r.book.buckets[n], 8);
  std::memcpy(p + 104, r.book.document, 33);
  std::memcpy(p + 137, r.book.title, 128);
  std::memcpy(p + 265, r.book.author, 64);
  p[329] = r.book.completed;
  write(p + 330, r.localDay, 4);
  write(p + 334, r.secondOfDay, 4);
  write(p + 338, r.book.finishedDay, 4);
  write(p + 342, r.reversedCompletionDay, 4);
  p[346] = r.book.manuallyUnfinished;
  write(p + kRecordBytes - 4, checksum(p, kRecordBytes - 4), 4);
}
bool decode(const uint8_t* p, Record& r) {
  if (std::memcmp(p, "RSE1", 4) || read(p + kRecordBytes - 4, 4) != checksum(p, kRecordBytes - 4)) return false;
  if (!std::memchr(p + 104, 0, 33) || !std::memchr(p + 137, 0, 128) || !std::memchr(p + 265, 0, 64)) return false;
  std::memcpy(r.book.document, p + 104, 33);
  std::memcpy(r.book.title, p + 137, 128);
  std::memcpy(r.book.author, p + 265, 64);
  r.seconds = static_cast<uint32_t>(read(p + 4, 4));
  r.pages = static_cast<uint32_t>(read(p + 8, 4));
  r.firstCompletion = p[12];
  r.kind = static_cast<RecordKind>(p[13]);
  r.book.progressBp = static_cast<uint16_t>(read(p + 14, 2));
  r.epoch = static_cast<int64_t>(read(p + 16, 8));
  r.book.seconds = read(p + 24, 8);
  r.book.sessions = static_cast<uint32_t>(read(p + 32, 4));
  r.book.pages = static_cast<uint32_t>(read(p + 36, 4));
  r.book.firstEpoch = static_cast<int64_t>(read(p + 40, 8));
  r.book.lastEpoch = static_cast<int64_t>(read(p + 48, 8));
  r.book.finishedEpoch = static_cast<int64_t>(read(p + 56, 8));
  r.book.readingDays = static_cast<uint32_t>(read(p + 64, 4));
  r.book.lastDay = static_cast<int>(read(p + 68, 4));
  for (int n = 0; n < 4; ++n) r.book.buckets[n] = read(p + 72 + n * 8, 8);
  r.book.completed = p[329];
  r.localDay = static_cast<int>(read(p + 330, 4));
  r.secondOfDay = static_cast<uint32_t>(read(p + 334, 4));
  r.book.finishedDay = static_cast<int>(read(p + 338, 4));
  r.reversedCompletionDay = static_cast<int>(read(p + 342, 4));
  r.book.manuallyUnfinished = p[346];
  const bool validKind =
      (r.kind == RecordKind::Session && r.seconds > 0 && r.book.sessions > 0) ||
      (r.kind == RecordKind::Completion && !r.seconds && !r.pages && r.book.progressBp == 10000 && r.book.completed) ||
      (r.kind == RecordKind::Reopen && !r.seconds && !r.pages && r.book.progressBp < 10000 && !r.book.completed &&
       r.book.manuallyUnfinished);
  return validKind && validDocument(r.book.document) && r.epoch > 0 && r.localDay >= 18262 && r.localDay <= 84005 &&
         r.secondOfDay < 86400 && r.book.progressBp <= 10000 && p[12] <= 1 && p[329] <= 1 && p[346] <= 1 &&
         (!r.book.manuallyUnfinished || !r.book.completed) &&
         (!r.book.finishedDay || (r.book.finishedDay >= 18262 && r.book.finishedDay <= 84005)) &&
         (!r.reversedCompletionDay || (r.reversedCompletionDay >= 18262 && r.reversedCompletionDay <= 84005));
}
}  // namespace readingstats
