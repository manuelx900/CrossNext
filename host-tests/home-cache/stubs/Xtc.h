#pragma once
#include <string>
class Xtc {
 public:
  Xtc(const std::string&, const char*) {}
  bool load() { return false; }
  std::string getCachePath() const { return ""; }
  unsigned getPageCount() const { return 100; }
  int calculateProgress(unsigned page) const { return int(page); }
};
