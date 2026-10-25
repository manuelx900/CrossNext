#pragma once

#include <cstddef>
#include <cstdint>

namespace habits {

constexpr int kMaxHabits = 12;
constexpr int kNameMax = 32;
constexpr int kHistoryDays = 400;
constexpr int kHistoryBytes = (kHistoryDays + 7) / 8;
constexpr int kMinDay = 18262;  // 2020-01-01, counted from the Unix epoch.
constexpr int kMaxDay = 47481;  // 2099-12-31.
constexpr size_t kFileSize = 16 + kMaxHabits * (kNameMax + 1 + 16 + kHistoryBytes) + 4;

struct Habit {
  char name[kNameMax + 1]{};
  int32_t createdDay = kMinDay;
  uint32_t archivedTotal = 0;
  uint32_t archivedBest = 0;
  uint32_t archivedRun = 0;
  uint8_t history[kHistoryBytes]{};
};

// Detailed history rolls over after 400 days; lifetime totals and streaks
// retain the completed prefix. Fixed storage avoids growth during check-ins.
struct Store {
  int32_t latestDay = -1;
  uint8_t count = 0;
  Habit items[kMaxHabits]{};
};

struct Stats {
  uint32_t current = 0;
  uint32_t best = 0;
  uint32_t total = 0;
  int recentDone = 0;
  int recentDays = 0;
};

enum class NameResult : uint8_t { Ok, Empty, TooLong, Unsupported, Duplicate, Full };

bool validDay(int day);
int dayIndex(int year, int month, int day);
void dateOfDay(int day, int& year, int& month, int& date);
bool parseDate(const char* text, int& day);
void formatDate(int day, char* out, size_t size);
int monthStart(int day);
int shiftMonth(int day, int step);
int weekday(int day);  // Monday = 0.
int firstDay(const Store& store);
bool checked(const Store& store, int index, int day);
bool advance(Store& store, int day);
NameResult nameResult(const Store& store, const char* name, int except = -1);
NameResult add(Store& store, const char* name, int day);
NameResult rename(Store& store, int index, const char* name);
bool remove(Store& store, int index);
bool toggle(Store& store, int index, int day, int today);
Stats stats(const Store& store, int index, int today);
int countOnDay(const Store&, int day);
int indexOnDay(const Store&, int day, int position);
int doneCount(const Store& store, int day);
bool encode(const Store& store, uint32_t generation, uint8_t* out, size_t size);
bool decode(const uint8_t* data, size_t size, Store& store, uint32_t& generation);
uint32_t checksum(const uint8_t* data, size_t size);

}  // namespace habits
