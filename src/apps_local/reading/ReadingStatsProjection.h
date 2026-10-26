#pragma once
#include <HalMemory.h>

#include "ReadingStatsCloud.h"
namespace readingstats::cloud {
struct Checkpoint {
  uint64_t identity = 0, length = 16, hash = 14695981039346656037ULL;
};
// One bounded PSRAM index; book bodies remain in the journal and SD snapshot.
class Projection {
 public:
  Result open(Reader* remote, int today, uint64_t device, const Header* acknowledged = nullptr);
  void close();
  const Header& data() const { return header; }
  const Checkpoint& checkpoint() const { return current; }
  uint32_t count() const { return countBooks; }
  bool bookAt(uint32_t ordinal, Book& out);
  bool find(const char* document, Book& out);
  bool page(BookFilter filter, int page, Book* out, int capacity, int& count);
  bool project(PeriodStats& out, bool* known = nullptr) const;
  uint64_t daySeconds(int day) const;
  uint64_t tod[4]{}, dow[7]{};

 private:
  static constexpr uint32_t kSlots = 32768, kDates = 36525;
  struct Entry {
    uint8_t document[16]{};
    uint32_t cloud = 0;
    uint64_t latest = 0, prior = 0;
    bool used = false, finished = false;
  };
  HalMemory::PsramBuffer indexMemory, orderMemory, dayMemory;
  Entry* entries = nullptr;
  uint32_t* order = nullptr;
  uint64_t* daily = nullptr;
  Reader* remote = nullptr;
  HalFile journal;
  Header header;
  Checkpoint current;
  Book base, latest, prior;
  Record record;
  uint8_t raw[kRecordBytes]{};
  uint32_t countBooks = 0;
  uint64_t cutoff = 16;
  Entry* locate(const char* document, bool create);
  bool read(uint64_t offset, Book& out);
  bool merge(const Entry& entry, Book& out);
};
}  // namespace readingstats::cloud
