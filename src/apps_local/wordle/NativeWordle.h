#pragma once

#include <HalStorage.h>
#include <I18n.h>

#include "LocalWordle.h"

namespace wordle {
// One active SD vocabulary and one fixed-size game. No word list is copied to
// RAM. Bank records contain NFC words and precomputed grapheme boundaries.
class NativeWordle {
 public:
  static constexpr unsigned MAX_UNITS = 6, WORD_BYTES = 97, RECORD_BYTES = 104, SAVE_BYTES = 1024;
  bool open(Language language);
  void close();
  bool available() const { return bool(bank); }
  bool supports(unsigned length) const { return length >= 2 && length <= 6 && counts[length - 2]; }
  unsigned letters() const { return length; }
  unsigned guesses() const { return rows; }
  bool active() const { return started; }
  bool over() const { return started && (won || rows == 6); }
  bool victory() const { return won; }
  bool persistenceFailed() const { return !saved; }
  Language language() const { return locale; }
  const localwordle::Record& statistics() const { return record; }
  bool start(unsigned letters, uint32_t random);
  bool accepts(unsigned letters, const char* text);
  bool acceptsBuiltin(unsigned letters, const char* text, uint8_t gameLanguage);
  bool submit(const char* text);
  const char* word(unsigned row) const { return words[row]; }
  Mark mark(unsigned row, unsigned col) const { return marks[row][col]; }
  Mark keyMark(const char* key) const;
  void unit(unsigned row, unsigned col, char out[16]) const;
  bool sample(unsigned letters, char out[WORD_BYTES], uint8_t boundaries[MAX_UNITS]);
  bool save();
  void inheritStatistics(const localwordle::Record& previous);

 private:
  HalFile bank;
  Language locale = Language::EN;
  uint32_t counts[5]{}, answers[5]{}, fingerprint = 0, sequence = 0;
  uint32_t answerOffset = 0;
  uint16_t headerBytes = 32, diskRecordBytes = RECORD_BYTES;
  unsigned length = 5, rows = 0;
  bool started = false, won = false, writable = true, saved = true;
  localwordle::Record record;
  char words[7][WORD_BYTES]{};  // six guesses, then the answer
  uint8_t ends[7][MAX_UNITS]{};
  Mark marks[6][MAX_UNITS]{};
  uint8_t io[SAVE_BYTES]{};  // reused for the two-slot save, never on the task stack
  bool readRecord(unsigned index, uint8_t out[RECORD_BYTES]);
  bool lookup(unsigned letters, const char* word, uint8_t out[RECORD_BYTES]);
  void score(unsigned row);
  void load();
  void path(char out[80]) const;
  bool saveError();
};
}  // namespace wordle
