#pragma once

#include <cstdint>
#include <string>

#include "ReadingStatsCore.h"

// Turns journal sessions into the per-page events a BookOrbit server builds its
// reading log from. The journal keeps one record per session, not per page, so a
// session's pages are spread evenly across its active seconds: totals and
// progress are exact, the timing of an individual page is not.
namespace readingstats {
inline constexpr const char* kPageStatsCursorPath = "/.crosspoint/reading-history/page-stats.bin";
inline constexpr uint32_t kMaxPageEvents = 100;
inline constexpr uint16_t kPageEventScale = 10000;  // positions are basis points of the book
struct PageEvent {
  uint16_t page = 0;
  int64_t startTime = 0;
  uint32_t durationSeconds = 0;
};
uint32_t pageEventCount(const Record& record);
PageEvent pageEvent(const Record& record, int startBp, uint32_t index, uint32_t count);
// Older records have no start position; `previous` is the journal's record before
// this one, which may belong to another book.
int sessionStartBp(const Record& record, const Record* previous);
// `post` receives the JSON `books` array and returns whether the server took it.
using PostPageStats = bool (*)(const std::string& books, void* user);
// Sends every session after the saved cursor. Returns false if a batch was refused;
// the batches before it stay sent and the next call resumes from there.
bool uploadPageStats(PostPageStats post, void* user);
}  // namespace readingstats
