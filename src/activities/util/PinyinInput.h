#pragma once
#include <HalStorage.h>
#include <I18n.h>

class PinyinInput {
 public:
  static constexpr unsigned CANDIDATES_PER_PAGE = 6;
  bool begin(Language language);
  void close();
  bool available() const { return count != 0; }
  const char* spelling() const { return composition; }
  const char* candidate(unsigned n) const { return n < CANDIDATES_PER_PAGE ? choices[n] : ""; }
  bool type(char letter);
  bool erase();
  bool canPage(int direction) const {
    return matches && (direction < 0 ? first != 0 : first + CANDIDATES_PER_PAGE < matches);
  }
  bool page(int direction);
  void clear();

 private:
  HalFile file;
  char composition[8]{};
  char choices[CANDIDATES_PER_PAGE][5]{};
  uint16_t count = 0, matches = 0, first = 0;
  uint32_t choicesOffset = 0;
  void find();
  void readPage();
};
