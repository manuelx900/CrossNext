#pragma once
#include <cstdint>

namespace bookdictionary {
struct Settings {
  uint8_t overrides = 0;
  char definition[32]{}, translation[32]{};
  const char* selected(bool translate, const char* system) const {
    return overrides & (translate ? 2 : 1) ? (translate ? translation : definition) : system;
  }
};
// Preferences live outside regenerable book caches. The stored full path is
// checked as well as the filename hash, so a collision cannot cross books.
bool load(const char* book, Settings& settings);
bool save(const char* book, const Settings& settings);
bool move(const char* from, const char* to);
}  // namespace bookdictionary
