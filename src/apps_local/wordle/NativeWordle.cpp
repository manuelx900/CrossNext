#include "NativeWordle.h"

#include <Logging.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "WordleCase.h"

namespace wordle {
namespace {
constexpr uint32_t MAX_COUNTER = 1000000;
constexpr uint32_t MAX_GUESSES_PER_LENGTH = 1000000;
void increment(uint32_t& value) { value = std::min(value, MAX_COUNTER - 1) + 1; }
uint32_t u32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
void put(uint8_t* p, uint32_t n) {
  for (unsigned i = 0; i < 4; ++i) p[i] = uint8_t(n >> (i * 8));
}
uint32_t digest(const uint8_t* p, unsigned n, uint32_t h = 2166136261u) {
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}
bool validRecord(const uint8_t* p, unsigned length) {
  if (p[97] != length || !p[0] || !std::memchr(p, 0, 97)) return false;
  unsigned previous = 0;
  for (unsigned i = 0; i < length; ++i) {
    const unsigned end = p[98 + i];
    if (end <= previous || end > 96 || end - previous > 15 || (p[previous] & 0xc0) == 0x80 || !p[previous])
      return false;
    const auto* cursor = p + previous;
    while (cursor < p + end) {
      const auto cp = utf8NextCodepoint(&cursor);
      if (!cp || cp == REPLACEMENT_GLYPH || cursor > p + end) return false;
    }
    previous = end;
  }
  return p[previous] == 0 && std::strlen(reinterpret_cast<const char*>(p)) == previous;
}
uint32_t canonicalCodepoint(uint32_t cp, Language language) {
  if (language == Language::TR || language == Language::AZ) {
    if (cp == 'I')
      cp = 0x131;
    else if (cp == 0x130)
      cp = 'i';
  }
  const auto* end = std::end(WORDLE_CASE_PAIRS);
  const auto* pair =
      std::lower_bound(std::begin(WORDLE_CASE_PAIRS), end, cp,
                       [](const WordleCasePair& entry, uint32_t codepoint) { return entry.upper < codepoint; });
  if (pair != end && pair->upper == cp) cp = pair->lower;
  if (language == Language::ES) {
    switch (cp) {
      case 0xe1:
        cp = 'a';
        break;
      case 0xe9:
        cp = 'e';
        break;
      case 0xed:
        cp = 'i';
        break;
      case 0xf3:
        cp = 'o';
        break;
      case 0xfa:
      case 0xfc:
        cp = 'u';
        break;
    }
  }
  return cp;
}
bool canonical(const char* value, Language language, char out[97]) {
  if (!value || std::strlen(value) > 192) return false;
  const auto text = utf8ComposeNfc(value);
  const auto* cursor = reinterpret_cast<const unsigned char*>(text.c_str());
  char* target = out;
  while (*cursor) {
    uint32_t cp = utf8NextCodepoint(&cursor);
    if (cp == REPLACEMENT_GLYPH || cp <= 0x20) return false;
    cp = canonicalCodepoint(cp, language);
    char encoded[4];
    const char* end = utf8AppendCodepoint(encoded, cp);
    if (target - out + (end - encoded) > 96) return false;
    std::memcpy(target, encoded, end - encoded);
    target += end - encoded;
  }
  *target = 0;
  return target != out;
}
}  // namespace

bool NativeWordle::open(Language language) {
  close();
  locale = language;
  char file[80];
  std::snprintf(file, sizeof(file), "/languages/wordle/%s.cpwb", LANGUAGE_CODES[unsigned(locale)]);
  if (!Storage.openFileForRead("WORDLE", file, bank)) return false;
  uint8_t header[64];
  if (bank.read(header, 32) != 32 || std::memcmp(header, "CPWB", 4) || header[5] || header[7] ||
      (header[4] != 1 && header[4] != 2) || header[6] != (header[4] == 1 ? 104 : 108)) {
    close();
    return false;
  }
  uint32_t total = 0;
  for (unsigned i = 0; i < 5; ++i) {
    counts[i] = u32(header + 8 + 4 * i);
    if (counts[i] > MAX_GUESSES_PER_LENGTH) {
      close();
      return false;
    }
    total += counts[i];
  }
  if (header[4] == 2) {
    if (bank.read(header + 32, 32) != 32 || digest(header, 52) != u32(header + 52)) {
      close();
      return false;
    }
    headerBytes = 64;
    diskRecordBytes = 108;
    answerOffset = total;
    for (unsigned i = 0; i < 5; ++i) {
      answers[i] = u32(header + 28 + 4 * i);
      if (answers[i] > counts[i]) {
        close();
        return false;
      }
      total += answers[i];
    }
    fingerprint = u32(header + 48);
  } else {
    std::memcpy(answers, counts, sizeof(answers));
    fingerprint = u32(header + 28);
  }
  if (!total || bank.size() != headerBytes + total * diskRecordBytes) {
    close();
    return false;
  }
  // V2 checks each accessed record. Opening a multi-megabyte bank stays O(1).
  // Legacy banks retain their whole-file integrity check.
  if (headerBytes == 32) {
    uint32_t hash = 2166136261u;
    while (bank.available()) {
      const int n = bank.read(io, sizeof(io));
      if (n <= 0) {
        close();
        return false;
      }
      hash = digest(io, n, hash);
    }
    if (hash != fingerprint) {
      LOG_ERR("WORDLE", "Invalid vocabulary checksum");
      close();
      return false;
    }
  }
  load();
  return true;
}
void NativeWordle::close() {
  bank.close();
  started = won = false;
  rows = sequence = 0;
  record = {};
  writable = true;
  saved = true;
  std::memset(counts, 0, sizeof(counts));
  std::memset(answers, 0, sizeof(answers));
  answerOffset = 0;
  headerBytes = 32;
  diskRecordBytes = RECORD_BYTES;
  std::memset(words, 0, sizeof(words));
  std::memset(ends, 0, sizeof(ends));
}
void NativeWordle::inheritStatistics(const localwordle::Record& previous) {
  if (record.played || !previous.played || !writable) return;
  record = previous;
  save();
}
bool NativeWordle::readRecord(unsigned index, uint8_t out[RECORD_BYTES]) {
  if (!bank.seekSet(headerBytes + uint32_t(index) * diskRecordBytes) || bank.read(out, RECORD_BYTES) != RECORD_BYTES)
    return false;
  if (diskRecordBytes == RECORD_BYTES) return true;
  uint8_t checksum[4];
  return bank.read(checksum, 4) == 4 && u32(checksum) == digest(out, RECORD_BYTES);
}
bool NativeWordle::lookup(unsigned size, const char* text, uint8_t out[RECORD_BYTES]) {
  if (!available() || !supports(size)) return false;
  char value[97];
  if (!canonical(text, locale, value)) return false;
  unsigned first = 0;
  for (unsigned i = 2; i < size; ++i) first += counts[i - 2];
  unsigned low = 0, high = counts[size - 2];
  while (low < high) {
    const unsigned mid = low + (high - low) / 2;
    if (!readRecord(first + mid, out) || !validRecord(out, size)) return false;
    const int comparison = std::strcmp(value, reinterpret_cast<const char*>(out));
    if (!comparison) return true;
    if (comparison < 0)
      high = mid;
    else
      low = mid + 1;
  }
  return false;
}
bool NativeWordle::accepts(unsigned size, const char* text) {
  uint8_t entry[RECORD_BYTES];
  return lookup(size, text, entry);
}
bool NativeWordle::acceptsBuiltin(unsigned size, const char* text, uint8_t gameLanguage) {
  if (locale != (gameLanguage ? Language::ES : Language::EN) || !supports(size) || !text) return false;
  char utf8[MAX_UNITS * 2 + 1];
  unsigned bytes = 0;
  for (unsigned i = 0; i < size; ++i) {
    if (text[i] < 'A' || text[i] > (gameLanguage ? '[' : 'Z')) return false;
    if (text[i] == '[') {
      utf8[bytes++] = char(0xc3);
      utf8[bytes++] = char(0x91);
    } else
      utf8[bytes++] = text[i];
  }
  utf8[bytes] = 0;
  return accepts(size, utf8);
}
bool NativeWordle::sample(unsigned size, char out[WORD_BYTES], uint8_t boundaries[MAX_UNITS]) {
  if (!supports(size) || !answers[size - 2]) return false;
  unsigned first = answerOffset;
  for (unsigned i = 2; i < size; ++i) first += answers[i - 2];
  uint8_t entry[RECORD_BYTES];
  // Corpus interjections can be admitted guesses. Use an answer for the menu,
  // skipping repeated-unit examples without scanning the whole vocabulary.
  for (unsigned candidate = 0; candidate < 16U && candidate < answers[size - 2]; ++candidate) {
    if (!readRecord(first + candidate, entry) || !validRecord(entry, size)) return false;
    bool varied = false;
    unsigned repeated = 1;
    for (unsigned unit = 1; unit < size; ++unit) {
      const unsigned from = entry[98 + unit - 1], to = entry[98 + unit];
      const unsigned previous = unit > 1 ? entry[98 + unit - 2] : 0;
      varied |= to - from != entry[98] || std::memcmp(entry, entry + from, to - from);
      repeated =
          to - from == from - previous && !std::memcmp(entry + previous, entry + from, to - from) ? repeated + 1 : 1;
      if (repeated >= 3) break;
    }
    if (!varied || repeated >= 3) continue;
    std::memcpy(out, entry, WORD_BYTES);
    std::memcpy(boundaries, entry + 98, MAX_UNITS);
    return true;
  }
  return false;
}
bool NativeWordle::start(unsigned size, uint32_t random) {
  if (!supports(size) || !answers[size - 2]) return false;
  unsigned first = answerOffset;
  for (unsigned i = 2; i < size; ++i) first += answers[i - 2];
  uint8_t entry[RECORD_BYTES];
  if (!readRecord(first + random % answers[size - 2], entry) || !validRecord(entry, size)) return false;
  if (started && !over()) {
    increment(record.played);
    record.streak = 0;
  }
  length = size;
  rows = 0;
  won = false;
  started = true;
  std::memset(words, 0, sizeof(words));
  std::memset(ends, 0, sizeof(ends));
  std::memcpy(words[6], entry, WORD_BYTES);
  std::memcpy(ends[6], entry + 98, MAX_UNITS);
  save();
  return true;
}
void NativeWordle::unit(unsigned row, unsigned col, char out[16]) const {
  out[0] = 0;
  if (row > 6 || col >= length) return;
  const unsigned from = col ? ends[row][col - 1] : 0, to = ends[row][col];
  if (to <= from || to - from > 15) return;
  std::memcpy(out, words[row] + from, to - from);
  out[to - from] = 0;
}
Mark NativeWordle::keyMark(const char* key) const {
  if (!key || !*key) return Mark::Empty;
  const auto* cursor = reinterpret_cast<const unsigned char*>(key);
  auto cp = utf8NextCodepoint(&cursor);
  cp = canonicalCodepoint(cp, locale);
  Mark best = Mark::Empty;
  for (unsigned row = 0; row < rows; ++row)
    for (unsigned col = 0; col < length; ++col) {
      const auto from = col ? ends[row][col - 1] : 0;
      const auto* at = reinterpret_cast<const unsigned char*>(words[row] + from);
      const auto* end = reinterpret_cast<const unsigned char*>(words[row] + ends[row][col]);
      while (at < end)
        if (utf8NextCodepoint(&at) == cp) best = std::max(best, marks[row][col]);
    }
  return best;
}
void NativeWordle::score(unsigned row) {
  bool used[MAX_UNITS]{};
  char guess[16], answer[16];
  for (unsigned col = 0; col < length; ++col) {
    unit(row, col, guess);
    unit(6, col, answer);
    marks[row][col] = std::strcmp(guess, answer) ? Mark::Absent : Mark::Correct;
    used[col] = marks[row][col] == Mark::Correct;
  }
  for (unsigned col = 0; col < length; ++col)
    if (marks[row][col] != Mark::Correct) {
      unit(row, col, guess);
      for (unsigned n = 0; n < length; ++n)
        if (!used[n]) {
          unit(6, n, answer);
          if (!std::strcmp(guess, answer)) {
            used[n] = true;
            marks[row][col] = Mark::Present;
            break;
          }
        }
    }
}
bool NativeWordle::submit(const char* text) {
  if (!started || over()) return false;
  uint8_t entry[RECORD_BYTES];
  if (!lookup(length, text, entry)) return false;
  std::memcpy(words[rows], entry, WORD_BYTES);
  std::memcpy(ends[rows], entry + 98, MAX_UNITS);
  score(rows++);
  won = !std::strcmp(words[rows - 1], words[6]);
  if (over()) {
    increment(record.played);
    if (won) {
      increment(record.won);
      increment(record.streak);
      record.maximum = std::max(record.maximum, record.streak);
      increment(record.distribution[rows - 1]);
    } else
      record.streak = 0;
  }
  save();
  return true;
}
void NativeWordle::path(char out[80]) const {
  std::snprintf(out, 80, "/.crosspoint/wordle/%s.bin", LANGUAGE_CODES[unsigned(locale)]);
}
void NativeWordle::load() {
  char file[80];
  path(file);
  HalFile save;
  if (!Storage.exists(file)) return;
  if (!Storage.openFileForRead("WORDLE", file, save)) {
    writable = false;
    saved = false;
    return;
  }
  int slot = -1;
  for (int n = 0; n < 2; ++n) {
    if (!save.seekSet(n * SAVE_BYTES) || save.read(io, SAVE_BYTES) != SAVE_BYTES) continue;
    if (!std::memcmp(io, "NW", 2) && std::memcmp(io + 2, "01", 2) > 0) {
      writable = saved = false;
      LOG_ERR("WORDLE", "Newer saved game version");
      return;
    }
    if (std::memcmp(io, "NW01", 4) || u32(io + 1020) != digest(io, 1020)) continue;
    if (slot < 0 || int32_t(u32(io + 4) - sequence) > 0) {
      sequence = u32(io + 4);
      slot = n;
    }
  }
  if (slot < 0) {
    writable = false;
    saved = false;
    LOG_ERR("WORDLE", "No valid saved game");
    return;
  }
  if (!save.seekSet(slot * SAVE_BYTES) || save.read(io, SAVE_BYTES) != SAVE_BYTES) {
    writable = false;
    saved = false;
    return;
  }
  // Fields are serialized individually rather than exposing the C++ object layout.
  record.played = u32(io + 16);
  record.won = u32(io + 20);
  record.streak = u32(io + 24);
  record.maximum = u32(io + 28);
  for (unsigned n = 0; n < 6; ++n) record.distribution[n] = u32(io + 32 + n * 4);
  if (io[13] > 6 || io[14] > 1 || !supports(io[12])) return;
  length = io[12];
  rows = io[13];
  started = io[14];
  won = false;
  if (!started) return;
  uint8_t entry[RECORD_BYTES];
  for (unsigned n = 0; n < 7; ++n)
    if (n < rows || n == 6) {
      const char* text = reinterpret_cast<const char*>(io + 64 + n * WORD_BYTES);
      if (!std::memchr(text, 0, WORD_BYTES) || !lookup(length, text, entry)) {
        started = false;
        return;
      }
      std::memcpy(words[n], entry, WORD_BYTES);
      std::memcpy(ends[n], entry + 98, MAX_UNITS);
    }
  for (unsigned n = 0; n < rows; ++n) score(n);
  won = rows && !std::strcmp(words[rows - 1], words[6]);
}
bool NativeWordle::save() {
  if (!writable || !Storage.ensureDirectoryExists("/.crosspoint") ||
      !Storage.ensureDirectoryExists("/.crosspoint/wordle"))
    return saveError();
  std::memset(io, 0, sizeof(io));
  std::memcpy(io, "NW01", 4);
  put(io + 4, sequence + 1);
  put(io + 8, fingerprint);
  io[12] = length;
  io[13] = rows;
  io[14] = started;
  put(io + 16, record.played);
  put(io + 20, record.won);
  put(io + 24, record.streak);
  put(io + 28, record.maximum);
  for (unsigned n = 0; n < 6; ++n) put(io + 32 + n * 4, record.distribution[n]);
  for (unsigned n = 0; n < 7; ++n) std::memcpy(io + 64 + n * WORD_BYTES, words[n], WORD_BYTES);
  put(io + 1020, digest(io, 1020));
  char file[80];
  path(file);
  HalFile save = Storage.open(file, O_RDWR | O_CREAT);
  if (!save) return saveError();
  const auto offset = sequence % 2 * SAVE_BYTES;
  if (!save.seekSet(offset) || save.write(io, SAVE_BYTES) != SAVE_BYTES) return saveError();
  save.flush();
  const uint32_t expected = u32(io + 1020);
  if (!save.seekSet(offset) || save.read(io, SAVE_BYTES) != SAVE_BYTES || u32(io + 1020) != expected ||
      digest(io, 1020) != expected)
    return saveError();
  ++sequence;
  saved = true;
  return true;
}
bool NativeWordle::saveError() {
  saved = false;
  LOG_ERR("WORDLE", "Could not persist game");
  return false;
}
}  // namespace wordle
