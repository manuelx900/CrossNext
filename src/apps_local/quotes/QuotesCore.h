#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace quotes {
inline void copyText(char* out, size_t cap, const char* value) {
  if (!cap) return;
  size_t n = std::min(std::strlen(value), cap - 1);
  while (n && (static_cast<unsigned char>(value[n]) & 0xc0) == 0x80) --n;
  std::memcpy(out, value, n);
  out[n] = 0;
}
inline constexpr uint64_t kHashStart = 1469598103934665603ull;
inline uint64_t hashByte(uint64_t value, uint8_t byte) { return (value ^ byte) * 1099511628211ull; }
inline uint64_t hash(const char* s) {
  uint64_t h = kHashStart;
  while (*s) h = hashByte(h, uint8_t(*s++));
  return h;
}
inline constexpr int kMax = 512;
inline constexpr uint8_t kFavorite = 1, kHidden = 2;
struct Entry {
  uint64_t key = 0, book = 0;
  uint32_t start = 0, end = 0;
  uint16_t page = 0;
  uint8_t flags = 0;
};
struct Quote {
  char title[192]{}, author[128]{}, date[80]{};
  char* body = nullptr;
  int page = 0;
};
inline void parse(char* record, Quote& q) {
  q = {};
  q.body = record;
  char* header = record;
  if (!std::strncmp(header, "\xef\xbb\xbf", 3)) header += 3;
  char* meta = std::strchr(header, '\n');
  if (!meta) return;
  *meta++ = 0;
  if (char* cr = std::strchr(header, '\r')) *cr = 0;
  char* author = nullptr;
  for (char* p = std::strstr(header, " ("); p; p = std::strstr(p + 2, " (")) author = p;
  if (author && header[std::strlen(header) - 1] == ')') {
    header[std::strlen(header) - 1] = 0;
    *author = 0;
    copyText(q.author, sizeof(q.author), author + 2);
  }
  copyText(q.title, sizeof(q.title), header);
  char* body = std::strchr(meta, '\n');
  if (!body) {
    q.body = meta;
    return;
  }
  *body++ = 0;
  const char* number = std::strstr(meta, "Page ");
  if (number) number += 5;
  if (!number) {
    number = std::strstr(meta, "page ");
    if (number) number += 5;
  }
  if (!number) {
    number = std::strstr(meta, "página ");
    if (number) number += 8;
  }
  if (number) q.page = std::atoi(number);
  if (const char* added = std::strstr(meta, "Added on ")) {
    static constexpr const char* months[] = {"January", "February", "March",     "April",   "May",      "June",
                                             "July",    "August",   "September", "October", "November", "December"};
    for (int month = 0; month < 12; ++month)
      if (const char* name = std::strstr(added, months[month])) {
        int day = 0, year = 0;
        if (std::sscanf(name + std::strlen(months[month]), " %d, %d", &day, &year) == 2 && day > 0 && day <= 31 &&
            year >= 2000 && year < 2100)
          std::snprintf(q.date, sizeof(q.date), "%02d/%02d/%04d", day, month + 1, year);
        break;
      }
  }
  while (*body == '\r' || *body == '\n') ++body;
  if (char* end = std::strstr(body, "\n==========")) *end = 0;
  size_t n = std::strlen(body);
  while (n && (body[n - 1] == '\r' || body[n - 1] == '\n')) body[--n] = 0;
  q.body = body;
}
}  // namespace quotes
