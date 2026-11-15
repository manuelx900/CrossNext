#pragma once

#include "ReaderBarsConfig.h"

class GfxRenderer;
namespace readerbars {
struct Data {
  const char* book = "";
  const char* chapter = "";
  float percent = 0;
  int page = 1, pages = 1;
  uint32_t timeBook = UNKNOWN_TIME, timeChapter = UNKNOWN_TIME;
  bool estimatedPages = false, bookmarked = false;
};
// Embedded in the existing fallible activity allocation, reused for both bars.
// Fixed text storage avoids per-paint strings and large stack frames.
struct Workspace {
  char text[SLOT_COUNT][64]{};
  std::array<int, SLOT_COUNT> widths{}, measured{};
};
void copyLabel(char (&out)[64], const char* source);
int barHeight(const Config& config);
void draw(GfxRenderer& renderer, const Config& config, Position position, const Data& data, Workspace& workspace,
          int padding = 0);
}  // namespace readerbars
