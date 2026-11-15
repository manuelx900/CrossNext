#pragma once

#include <cstdint>

// Activity/render-owned summaries. Unknown external writes invalidate the source stamp.
namespace readingtime {
bool lookup(const char* path, uint64_t& seconds, bool& known);
void remember(const char* path, const char* document, uint64_t seconds, bool known, uint32_t resourceBefore,
              uint32_t mediaBefore, uint32_t journalBefore);
void publish(const char* document, uint64_t seconds);
}  // namespace readingtime
