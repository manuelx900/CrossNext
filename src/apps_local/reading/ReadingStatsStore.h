#pragma once

#include <HalStorage.h>

#include "ReadingStatsCore.h"

namespace readingstats {
inline constexpr const char* kJournalPath = "/.crosspoint/reading-history/sessions.bin";
inline constexpr uint8_t kJournalHeader[16] = {'C', 'R', 'S', '1', 0x80, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
enum class ScanResult { More, Record, Finished, Error };
class Repository {
 public:
  bool begin(bool reverse = false, uint8_t* readAhead = nullptr, uint32_t readAheadBytes = 0);
  ScanResult next(Record& record);
  void close();
  bool record(const char* document, const char* title, const char* author, int64_t epoch, uint32_t seconds,
              uint32_t pages, uint16_t progressBp);
  bool loadBook(const char* document, Book& book);
  bool complete(const char* document, const char* title, const char* author, int64_t epoch);
  bool hadDamage() const { return damaged; }

 private:
  HalFile file;
  uint8_t buffer[kRecordBytes]{};
  Record scratch;
  Book previous;
  uint64_t cursor = 0, end = 0;
  bool backwards = false, damaged = false;
  uint8_t* readAhead = nullptr;
  uint32_t readAheadCapacity = 0, bufferedBytes = 0;
  uint64_t bufferedAt = 0;
  bool readAt(uint64_t offset, Record& record);
  bool header(HalFile& input);
  bool append(const Record& record);
};
int today();
bool recordSession(const char* document, const char* title, const char* author, int64_t epoch, uint32_t seconds,
                   uint32_t pages, uint16_t progressBp);
bool recordCompletion(const char* document, const char* title, const char* author, int64_t epoch);
}  // namespace readingstats
