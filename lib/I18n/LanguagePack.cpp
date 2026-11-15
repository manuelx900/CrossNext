#include "LanguagePack.h"

#include <Arduino.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "I18nKeys.h"
#include "I18nPackKeys.h"
#include "I18nStrings.h"

namespace languagepack {
namespace {
constexpr size_t HEADER_BYTES = 112;
constexpr size_t ENTRY_BYTES = 16;
constexpr size_t STRING_COUNT = static_cast<size_t>(StrId::_COUNT);
constexpr size_t OFFSET_BYTES = STRING_COUNT * sizeof(uint32_t);
constexpr uint32_t MISSING = UINT32_MAX;

uint16_t read16(const uint8_t* p) { return p[0] | (static_cast<uint16_t>(p[1]) << 8); }
uint32_t read32(const uint8_t* p) { return read16(p) | (static_cast<uint32_t>(read16(p + 2)) << 16); }
uint64_t read64(const uint8_t* p) { return read32(p) | (static_cast<uint64_t>(read32(p + 4)) << 32); }

bool asciiAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool asciiDigit(char c) { return c >= '0' && c <= '9'; }

bool skipFormatDigits(const char*& text) {
  unsigned value = 0;
  while (asciiDigit(*text)) {
    value = value * 10 + static_cast<unsigned>(*text++ - '0');
    if (value > 128) return false;
  }
  return true;
}

bool validUtf8(const uint8_t* p, size_t size, bool multiline) {
  size_t index = 0;
  while (index < size) {
    const uint8_t lead = p[index++];
    if (lead < 0x80) {
      if (!lead || (lead < 32 && (!multiline || (lead != '\n' && lead != '\r' && lead != '\t')))) return false;
      continue;
    }
    uint32_t code = 0;
    uint32_t minimum = 0;
    size_t trailing = 0;
    if (lead >= 0xc2 && lead <= 0xdf) {
      trailing = 1;
      code = lead & 0x1f;
      minimum = 0x80;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      trailing = 2;
      code = lead & 0x0f;
      minimum = 0x800;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      trailing = 3;
      code = lead & 7;
      minimum = 0x10000;
    } else {
      return false;
    }
    if (trailing > size - index) return false;
    while (trailing--) {
      const uint8_t next = p[index++];
      if ((next & 0xc0) != 0x80) return false;
      code = (code << 6) | (next & 0x3f);
    }
    if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return false;
  }
  return true;
}

uint32_t crc32(uint32_t crc, const uint8_t* bytes, size_t size) {
  while (size--) {
    crc ^= *bytes++;
    for (uint8_t bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
  }
  return crc;
}

size_t stringId(uint64_t hash) {
  const auto* begin = i18n_strings::PACK_KEY_HASHES;
  const auto* end = begin + STRING_COUNT;
  const auto* found = std::lower_bound(begin, end, hash);
  return found != end && *found == hash ? i18n_strings::PACK_KEY_IDS[found - begin] : STRING_COUNT;
}

struct Argument {
  uint8_t widthStars = 0;
  uint8_t length = 0;
  char kind = 0;
  bool operator==(const Argument& rhs) const {
    return widthStars == rhs.widthStars && length == rhs.length && kind == rhs.kind;
  }
};

bool nextArgument(const char*& text, Argument& arg) {
  arg = {};
  while (*text) {
    if (*text++ != '%') continue;
    if (*text == '%') {
      ++text;
      continue;
    }
    while (*text && strchr("-+ #0", *text)) ++text;
    if (*text == '*') {
      ++arg.widthStars;
      ++text;
    } else {
      if (!skipFormatDigits(text)) return false;
    }
    if (*text == '.') {
      ++text;
      if (*text == '*') {
        ++arg.widthStars;
        ++text;
      } else {
        if (!skipFormatDigits(text)) return false;
      }
    }
    if (*text && strchr("hljztL", *text)) {
      const char first = *text++;
      arg.length = static_cast<uint8_t>(first);
      if ((first == 'h' || first == 'l') && *text == first) {
        arg.length |= 0x80;
        ++text;
      }
    }
    if (!*text || !strchr("diouxXfFeEgGaAcsp", *text)) return false;
    const char conversion = *text++;
    arg.kind = strchr("di", conversion)         ? 'd'
               : strchr("ouxX", conversion)     ? 'u'
               : strchr("fFeEgGaA", conversion) ? 'f'
                                                : conversion;
    return true;
  }
  return true;
}

int plainPercentCount(const char* text) {
  int count = 0;
  for (const char* p = text; *p; ++p) {
    if (*p != '%') continue;
    Argument arg;
    const char* probe = p;
    if (nextArgument(probe, arg) && arg.kind) return -1;
    if (asciiAlpha(p[1]) || asciiDigit(p[1]) || p[1] == '%' || p[1] == '*' || p[1] == '$') return -1;
    ++count;
  }
  return count;
}

bool makePath(char* path, size_t size, const char* filename) {
  if (!validFilename(filename)) return false;
  const int length = snprintf(path, size, "/languages/%s", filename);
  return length > 0 && static_cast<size_t>(length) < size;
}

bool openPack(const char* filename, HalFile& file) {
  char path[80];
  return makePath(path, sizeof(path), filename) && Storage.openFileForRead("I18N", path, file);
}
}  // namespace

bool validFilename(const char* filename) {
  if (!filename) return false;
  const size_t length = strnlen(filename, FILENAME_BYTES);
  if (length <= 7 || length >= FILENAME_BYTES || strstr(filename, "..") ||
      strcmp(filename + length - 7, ".cplang") != 0)
    return false;
  for (size_t i = 0; i < length; ++i) {
    const char c = filename[i];
    if (!asciiAlpha(c) && !asciiDigit(c) && c != '-' && c != '_' && c != '.') return false;
  }
  return true;
}

bool readInfo(HalFile& file, Info& info) {
  uint8_t header[HEADER_BYTES];
  if (!file.seek(0) || file.read(header, sizeof(header)) != sizeof(header)) return false;
  if (memcmp(header, "CPLANG01", 8) || read16(header + 8) != 1 || read16(header + 10) != 0) return false;
  info.entries = read32(header + 12);
  info.textBytes = read32(header + 16);
  info.checksum = read32(header + 20);
  if (info.entries > MAX_ENTRIES || info.textBytes > MAX_TEXT_BYTES ||
      file.fileSize64() != HEADER_BYTES + info.entries * ENTRY_BYTES + info.textBytes || !memchr(header + 24, 0, 24) ||
      !memchr(header + 48, 0, 64))
    return false;
  memcpy(info.code, header + 24, sizeof(info.code));
  memcpy(info.name, header + 48, sizeof(info.name));
  if (!asciiAlpha(info.code[0]) || !info.name[0]) return false;
  for (size_t i = 1; info.code[i]; ++i) {
    if (!asciiAlpha(info.code[i]) && !asciiDigit(info.code[i]) && info.code[i] != '-') return false;
  }
  return validUtf8(reinterpret_cast<const uint8_t*>(info.name), strlen(info.name), false);
}

bool formatsMatch(const char* english, const char* translation) {
  const char* probe = english;
  Argument arg;
  bool hasArguments = false;
  bool valid = true;
  for (;;) {
    if (!nextArgument(probe, arg)) {
      valid = false;
      break;
    }
    if (!arg.kind) break;
    hasArguments = true;
  }
  if (!valid && !hasArguments) {
    const int count = plainPercentCount(english);
    return count >= 0 && plainPercentCount(translation) == count;
  }
  Argument original, translated;
  do {
    if (!nextArgument(english, original) || !nextArgument(translation, translated) || !(original == translated))
      return false;
  } while (original.kind);
  return true;
}

bool Pack::load(const char* filename) {
  if (arena) {
    LOG_ERR("I18N", "Language pack already loaded; restart to replace it");
    return false;
  }
  HalFile file;
  Info candidate;
  if (!openPack(filename, file) || !readInfo(file, candidate)) {
    LOG_ERR("I18N", "Missing or invalid language pack header");
    return false;
  }
  // One boot-lifetime arena: O(1) string offsets and UTF-8 text; no internal-RAM fallback.
  auto buffer = HalMemory::allocatePsram(OFFSET_BYTES + candidate.textBytes);
  if (!buffer) {
    LOG_ERR("I18N", "No PSRAM for language pack: %u bytes", static_cast<unsigned>(OFFSET_BYTES + candidate.textBytes));
    return false;
  }
  memset(buffer.get(), 0xff, OFFSET_BYTES);
  auto* text = buffer.get() + OFFSET_BYTES;
  if (!file.seek(HEADER_BYTES + candidate.entries * ENTRY_BYTES)) {
    LOG_ERR("I18N", "Language text seek failed");
    return false;
  }
  for (size_t offset = 0; offset < candidate.textBytes;) {
    const size_t length = std::min<size_t>(4096, candidate.textBytes - offset);
    if (file.read(text + offset, length) != static_cast<int>(length)) {
      LOG_ERR("I18N", "Short language pack read");
      return false;
    }
    offset += length;
    delay(0);
  }
  if (!file.seek(HEADER_BYTES)) {
    LOG_ERR("I18N", "Language table seek failed");
    return false;
  }
  uint64_t previousHash = 0;
  uint32_t expectedOffset = 0;
  uint32_t crc = UINT32_MAX;
  for (uint32_t index = 0; index < candidate.entries; ++index) {
    uint8_t entry[ENTRY_BYTES];
    if (file.read(entry, sizeof(entry)) != sizeof(entry)) {
      LOG_ERR("I18N", "Short language table read");
      return false;
    }
    crc = crc32(crc, entry, sizeof(entry));
    const uint64_t hash = read64(entry);
    const uint32_t offset = read32(entry + 8), length = read32(entry + 12);
    if ((index && hash <= previousHash) || offset != expectedOffset || offset >= candidate.textBytes || !length ||
        length > MAX_STRING_BYTES || length >= candidate.textBytes - offset || text[offset + length] != 0 ||
        !validUtf8(text + offset, length, true)) {
      LOG_ERR("I18N", "Invalid language pack entry: %u", static_cast<unsigned>(index));
      return false;
    }
    expectedOffset = offset + length + 1;
    previousHash = hash;
    const size_t id = stringId(hash);
    if (id < STRING_COUNT) {
      const char* english = i18n_strings::STRINGS_EN_DATA + i18n_strings::OFFSETS_EN[id];
      if (!formatsMatch(english, reinterpret_cast<const char*>(text + offset))) {
        LOG_ERR("I18N", "Unsafe translation format: %u", static_cast<unsigned>(id));
        return false;
      }
      memcpy(buffer.get() + id * sizeof(offset), &offset, sizeof(offset));
    }
    if ((index & 63) == 0) delay(0);
  }
  for (size_t offset = 0; offset < candidate.textBytes; offset += 4096) {
    crc = crc32(crc, text + offset, std::min<size_t>(4096, candidate.textBytes - offset));
    delay(0);
  }
  if (expectedOffset != candidate.textBytes || (crc ^ UINT32_MAX) != candidate.checksum) {
    LOG_ERR("I18N", "Language pack checksum mismatch");
    return false;
  }
  metadata = candidate;
  arena = std::move(buffer);
  LOG_INF("I18N", "Loaded language %s: %u translations, %u PSRAM bytes", metadata.code,
          static_cast<unsigned>(metadata.entries), static_cast<unsigned>(OFFSET_BYTES + metadata.textBytes));
  return true;
}

const char* Pack::get(size_t id) const {
  if (!arena || id >= STRING_COUNT) return nullptr;
  uint32_t offset;
  memcpy(&offset, arena.get() + id * sizeof(offset), sizeof(offset));
  return offset == MISSING ? nullptr : reinterpret_cast<const char*>(arena.get() + OFFSET_BYTES + offset);
}

const CatalogEntry& Catalog::operator[](size_t index) const {
  return reinterpret_cast<const CatalogEntry*>(entries.get())[index];
}

bool Catalog::scan() {
  count = 0;
  entries.reset();
  auto directory = Storage.open("/languages");
  if (!directory || !directory.isDirectory()) return true;
  bool ioError = false;
  size_t capacity = 0;
  char filename[FILENAME_BYTES];
  while (auto file = directory.openNextFile(ioError)) {
    if (!file.isDirectory() && file.getName(filename, sizeof(filename)) < sizeof(filename) && validFilename(filename)) {
      if (++capacity == MAX_INSTALLED) break;
    }
    delay(0);
  }
  if (ioError) {
    LOG_ERR("I18N", "Language directory read failed");
    return false;
  }
  if (!capacity) return true;
  // The catalog exists only while choosing a language; visible rows borrow its fixed records.
  entries = HalMemory::allocatePsram(capacity * sizeof(CatalogEntry));
  if (!entries) {
    LOG_ERR("I18N", "No PSRAM for language catalog");
    return false;
  }
  memset(entries.get(), 0, capacity * sizeof(CatalogEntry));
  directory.rewindDirectory();
  auto* records = reinterpret_cast<CatalogEntry*>(entries.get());
  while (count < capacity) {
    auto file = directory.openNextFile(ioError);
    if (!file) break;
    delay(0);
    if (file.isDirectory() || file.getName(filename, sizeof(filename)) >= sizeof(filename) || !validFilename(filename))
      continue;
    if (readInfo(file, records[count].info)) {
      memcpy(records[count].filename, filename, strlen(filename) + 1);
      ++count;
    }
    delay(0);
  }
  if (ioError) {
    LOG_ERR("I18N", "Language catalog read failed");
    count = 0;
    entries.reset();
    return false;
  }
  std::sort(records, records + count, [](const CatalogEntry& a, const CatalogEntry& b) {
    const int codeOrder = strcmp(a.info.code, b.info.code);
    return codeOrder ? codeOrder < 0 : strcmp(a.filename, b.filename) < 0;
  });
  return true;
}
}  // namespace languagepack
