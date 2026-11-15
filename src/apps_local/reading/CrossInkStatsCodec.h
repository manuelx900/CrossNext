#pragma once

#include <cstddef>
#include <cstdint>

#include "ReadingStatsCore.h"

namespace readingstats::crossink {
// CrossInk's epoch days start at 2000-01-01. No dated minute totals are stored.
constexpr int kEpochDay = 10957;
struct Global {
  uint32_t sessions = 0, seconds = 0, pages = 0, completed = 0;
  uint32_t longest = 0;
  int anchor = 0;
  uint8_t active[92]{};
};
inline uint32_t little(const uint8_t* p, size_t count) {
  uint32_t v = 0;
  for (size_t n = 0; n < count; ++n) v |= uint32_t(p[n]) << (8 * n);
  return v;
}
inline bool decodeGlobal(const uint8_t* p, size_t bytes, Global& out) {
  if (!p || !bytes || p[0] < 1 || p[0] > 3) return false;
  constexpr size_t sizes[] = {0, 13, 17, 159};
  if (bytes != sizes[p[0]]) return false;
  out = {};
  out.sessions = little(p + 1, 4);
  out.seconds = little(p + 5, 4);
  out.pages = little(p + 9, 4);
  if (p[0] >= 2) out.completed = little(p + 13, 4);
  if (p[0] == 3) {
    const auto anchor = little(p + 61, 4);
    if (anchor > 36524) return false;
    out.anchor = kEpochDay + anchor;
    for (size_t n = 0; n < sizeof(out.active); ++n) out.active[n] = p[65 + n];
    out.active[91] &= 3;
    out.longest = little(p + 157, 2);
  }
  return true;
}
inline bool decodeBook(const uint8_t* p, size_t bytes, Book& out) {
  if (!p || !bytes || p[0] < 1 || p[0] > 5) return false;
  constexpr size_t sizes[] = {0, 11, 12, 16, 69, 73};
  if (bytes != sizes[p[0]] || (p[0] >= 2 && p[11] > 1)) return false;
  out = {};
  out.sessions = little(p + 1, 2);
  out.seconds = little(p + 3, 4);
  out.pages = little(p + 7, 4);
  out.completed = p[0] >= 2 && p[11];
  out.progressBp = out.completed ? 10000 : 0;
  if (p[0] >= 4) {
    const int start = dayIndex({int(little(p + 17, 2)), p[19], p[20]});
    const int finish = dayIndex({int(little(p + 21, 2)), p[23], p[24]});
    out.firstDay = start;
    if (out.completed && finish) {
      out.finishedDay = finish;
    }
    // CrossInk's time-of-day intervals differ from ours; do not relabel them.
  }
  return true;
}
}  // namespace readingstats::crossink
