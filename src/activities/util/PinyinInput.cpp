#include "PinyinInput.h"

#include <Logging.h>
#include <Utf8.h>

#include <cstring>

namespace {
uint32_t read32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint16_t read16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
}  // namespace

bool PinyinInput::begin(Language language) {
  close();
  if (language != Language::ZH_CN && language != Language::ZH_TW) return false;
  const char* path =
      language == Language::ZH_CN ? "/languages/keyboards/pinyin-sc.bin" : "/languages/keyboards/pinyin-tc.bin";
  uint8_t header[16];
  if (!Storage.openFileForRead("PINYIN", path, file)) return false;
  if (file.read(header, sizeof(header)) != sizeof(header) || std::memcmp(header, "CPPY", 4) ||
      read16(header + 4) != 1 || read16(header + 6) == 0 || read16(header + 6) > 1024 ||
      read32(header + 8) != file.size() || file.size() < 16 + uint32_t(read16(header + 6)) * 16 ||
      file.size() > 256 * 1024) {
    LOG_ERR("PINYIN", "Invalid input dictionary");
    close();
    return false;
  }
  count = read16(header + 6);
  return true;
}

void PinyinInput::close() {
  file.close();
  count = 0;
  clear();
}

void PinyinInput::clear() {
  composition[0] = 0;
  std::memset(choices, 0, sizeof(choices));
  matches = first = 0;
}

bool PinyinInput::type(char letter) {
  if (letter >= 'A' && letter <= 'Z') letter += 'a' - 'A';
  const size_t n = std::strlen(composition);
  if (!available() || letter < 'a' || letter > 'z' || n + 1 >= sizeof(composition)) return false;
  composition[n] = letter;
  composition[n + 1] = 0;
  find();
  return true;
}

bool PinyinInput::erase() {
  const size_t n = std::strlen(composition);
  if (!n) return false;
  composition[n - 1] = 0;
  find();
  return true;
}

void PinyinInput::find() {
  first = matches = 0;
  std::memset(choices, 0, sizeof(choices));
  if (!composition[0]) return;
  unsigned low = 0, high = count;
  uint8_t row[16];
  while (low < high) {
    const unsigned mid = (low + high) / 2;
    if (!file.seekSet(16 + mid * sizeof(row)) || file.read(row, sizeof(row)) != sizeof(row) || row[7] != 0) return;
    const int result = std::strcmp(reinterpret_cast<const char*>(row), composition);
    if (result < 0)
      low = mid + 1;
    else if (result > 0)
      high = mid;
    else {
      choicesOffset = read32(row + 8);
      const uint16_t found = read16(row + 12);
      if (found > 2048 || choicesOffset < 16 + uint32_t(count) * 16 || choicesOffset > file.size() ||
          uint32_t(found) * 4 > file.size() - choicesOffset)
        return;
      matches = found;
      readPage();
      return;
    }
  }
}

bool PinyinInput::page(int direction) {
  if (!canPage(direction)) return false;
  first = direction < 0 ? first - CANDIDATES_PER_PAGE : first + CANDIDATES_PER_PAGE;
  readPage();
  return true;
}

void PinyinInput::readPage() {
  std::memset(choices, 0, sizeof(choices));
  uint8_t bytes[CANDIDATES_PER_PAGE * 4];
  const unsigned n = matches - first < CANDIDATES_PER_PAGE ? matches - first : CANDIDATES_PER_PAGE;
  if (!file.seekSet(choicesOffset + first * 4) || file.read(bytes, n * 4) != int(n * 4)) return;
  for (unsigned i = 0; i < n; ++i) {
    const uint32_t cp = read32(bytes + i * 4);
    if (cp < 0x3400 || cp > 0x9fff) continue;
    choices[i][0] = char(0xe0 | cp >> 12);
    choices[i][1] = char(0x80 | ((cp >> 6) & 0x3f));
    choices[i][2] = char(0x80 | (cp & 0x3f));
  }
}
