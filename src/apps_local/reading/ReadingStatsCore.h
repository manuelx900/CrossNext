#pragma once

#include <cstddef>
#include <cstdint>

namespace readingstats {
constexpr size_t kRecordBytes = 384;
constexpr int kHistoryDays = 730;
constexpr int kBooksPerPage = 5;
constexpr int kRecentBooks = 10;
enum class Period { Calendar, Day, Week, Month, Year };
enum class BookFilter { Finished, Recent };
enum class Source { Local, Unified };
struct Date {
  int year = 0, month = 0, day = 0;
};
int dayIndex(Date date);
Date dateFromDay(int day);
int daysInMonth(int year, int month);
int weekday(int day);
struct Book {
  char document[33]{};
  char title[128]{};
  char author[64]{};
  uint64_t seconds = 0;
  uint64_t buckets[4]{};
  int64_t firstEpoch = 0, lastEpoch = 0, finishedEpoch = 0;
  uint32_t sessions = 0, pages = 0, readingDays = 0;
  int lastDay = 0;
  uint16_t progressBp = 0;
  bool completed = false;
};
struct Record {
  Book book;
  int64_t epoch = 0;
  uint32_t seconds = 0, pages = 0;
  int localDay = 0;
  uint32_t secondOfDay = 0;
  bool firstCompletion = false;
};
struct Summary {
  uint64_t seconds = 0, pages = 0;
  uint32_t sessions = 0, completed = 0, totalDays = 0;
  uint32_t dailySeconds[kHistoryDays]{};
  int anchorDay = 0;
  int lastReadDay = 0;
  uint32_t run = 0, longest = 0;
  void add(const Record& record);
  uint32_t onDay(int day) const;
  uint32_t currentStreak(int today) const;
  uint32_t daysRead() const;
};
struct Streak {
  int first = 0, last = 0, minimumDay = 0;
  void begin(int through);
  void add(const Record& record);
  uint32_t days() const;
};
// A selected calendar period, accumulated from the existing session journal.
struct PeriodStats {
  int first = 0, last = 0;
  uint64_t seconds = 0, pages = 0;
  uint32_t sessions = 0, completed = 0;
  uint64_t hours[6]{};
  uint64_t dailySeconds[31]{};
  // Annual projection uses twelve totals and one bit per date, not 366 counters.
  uint64_t annualSeconds[12]{}, annualDays[6]{};
  uint32_t annualCompleted[12]{};
  bool annual = false;
  int daysReadThrough(int day) const;
  Streak consistency;
  void add(const Record& record);
};
int weekStart(int day, bool sunday);
int isoWeek(int day);
void setPeriod(PeriodStats& stats, Period period, int day, bool sunday);
// Most recently appended unfinished snapshots; a completion removes the book.
void addRecent(Book* books, int& count, const Book& book);
uint8_t formatMask(const char* path);
bool enabledFormat(const char* path, uint8_t mask);
bool validDocument(const char* document);
void updateBook(Record& record, const Book& previous);
void encode(const Record& record, uint8_t* out);
bool decode(const uint8_t* bytes, Record& record);
uint32_t checksum(const uint8_t* bytes, size_t length);
void copyText(char* out, size_t size, const char* text);
}  // namespace readingstats
