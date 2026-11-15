#include "BookTextPreferences.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>
#include <cstring>
#include <functional>

#include "CrossPointSettings.h"

namespace booktext {
namespace {
#define TEXT_FIELDS(X)                              \
  X(fontFamily, "fontFamily")                       \
  X(fontPointSize, "fontSize")                      \
  X(fontWeight, "fontWeight")                       \
  X(lineSpacing, "lineHeight")                      \
  X(paragraphAlignment, "paragraphAlignment")       \
  X(extraParagraphSpacing, "extraParagraphSpacing") \
  X(paragraphIndentSpaces, "paragraphIndentSpaces") \
  X(wordSpacing, "wordSpacing")                     \
  X(characterSpacing, "characterSpacing")           \
  X(screenMargin, "screenMargin")                   \
  X(hyphenationEnabled, "hyphenationEnabled")       \
  X(embeddedStyle, "embeddedStyle")                 \
  X(focusReadingEnabled, "focusReadingEnabled")     \
  X(textAntiAliasing, "textAntiAliasing")
#define INDEX(member, key) member,
enum Field { TEXT_FIELDS(INDEX) Count };
#undef INDEX
struct Preferences {
  uint8_t values[Count]{};
  char family[32]{};
};
// Fixed, bounded metadata; no page buffer or additional font cache is retained.
Preferences defaults, saved;
bool active = false, haveSaved = false;
unsigned activeSlot = 1;
uint32_t sequence = 0;
char directory[80]{};
constexpr size_t RECORD_SIZE = 4 + 4 + Count + 32 + 4;
Preferences capture() {
  Preferences p;
#define COPY(member, key) p.values[member] = SETTINGS.member;
  TEXT_FIELDS(COPY)
#undef COPY
  std::memcpy(p.family, SETTINGS.sdFontFamilyName, sizeof(p.family));
  return p;
}
void apply(const Preferences& p) {
#define COPY(member, key) SETTINGS.member = p.values[member];
  TEXT_FIELDS(COPY)
#undef COPY
  std::memcpy(SETTINGS.sdFontFamilyName, p.family, sizeof(p.family));
}
bool valid(const Preferences& p) {
  const auto* v = p.values;
  return std::memchr(p.family, 0, sizeof(p.family)) && !std::strchr(p.family, '/') && !std::strchr(p.family, '\\') &&
         v[fontFamily] < CrossPointSettings::BUILTIN_FONT_COUNT && v[fontPointSize] >= 8 && v[fontPointSize] <= 72 &&
         v[fontWeight] < CrossPointSettings::FONT_WEIGHT_COUNT &&
         v[lineSpacing] < CrossPointSettings::LINE_COMPRESSION_COUNT &&
         v[paragraphAlignment] < CrossPointSettings::PARAGRAPH_ALIGNMENT_COUNT && v[extraParagraphSpacing] <= 1 &&
         v[paragraphIndentSpaces] <= 5 && v[wordSpacing] >= 50 && v[wordSpacing] <= 200 && v[characterSpacing] <= 4 &&
         v[screenMargin] >= CrossPointSettings::SCREEN_MARGIN_MIN &&
         v[screenMargin] <= CrossPointSettings::SCREEN_MARGIN_MAX && v[hyphenationEnabled] <= 1 &&
         v[embeddedStyle] <= 1 && v[focusReadingEnabled] <= 1 && v[textAntiAliasing] <= 1;
}
uint32_t digest(const uint8_t* bytes, size_t n) {
  uint32_t hash = 2166136261u;
  while (n--) hash = (hash ^ *bytes++) * 16777619u;
  return hash;
}
void path(unsigned slot, char (&out)[96]) { std::snprintf(out, sizeof(out), "%s/text.%u.bin", directory, slot); }
bool read(unsigned slot, Preferences& p, uint32_t& serial) {
  char name[96];
  path(slot, name);
  if (!Storage.exists(name)) return false;
  HalFile file;
  uint8_t record[RECORD_SIZE];
  if (!Storage.openFileForRead("BTEXT", name, file) || file.size() != sizeof(record) ||
      file.read(record, sizeof(record)) != sizeof(record) || std::memcmp(record, "BTX1", 4))
    return false;
  serial = 0;
  uint32_t hash = 0;
  for (unsigned n = 0; n < 4; ++n) {
    serial |= uint32_t(record[4 + n]) << (n * 8);
    hash |= uint32_t(record[RECORD_SIZE - 4 + n]) << (n * 8);
  }
  if (hash != digest(record, RECORD_SIZE - 4)) return false;
  std::memcpy(p.values, record + 8, Count);
  std::memcpy(p.family, record + 8 + Count, sizeof(p.family));
  return valid(p);
}
bool writeBook(const Preferences& p) {
  if (!valid(p)) {
    LOG_ERR("BTEXT", "Invalid text preferences");
    return false;
  }
  uint8_t record[RECORD_SIZE]{};
  std::memcpy(record, "BTX1", 4);
  const uint32_t serial = sequence + 1;
  for (unsigned n = 0; n < 4; ++n) record[4 + n] = uint8_t(serial >> (n * 8));
  std::memcpy(record + 8, p.values, Count);
  std::memcpy(record + 8 + Count, p.family, sizeof(p.family));
  const auto hash = digest(record, RECORD_SIZE - 4);
  for (unsigned n = 0; n < 4; ++n) record[RECORD_SIZE - 4 + n] = uint8_t(hash >> (n * 8));
  char name[96];
  path(1 - activeSlot, name);
  bool written = false;
  if (Storage.ensureDirectoryExists(directory)) {
    HalFile file;
    written = Storage.openFileForWrite("BTEXT", name, file) && file.write(record, sizeof(record)) == sizeof(record);
  }
  Preferences verified;
  uint32_t verifiedSerial = 0;
  if (!written || !read(1 - activeSlot, verified, verifiedSerial) || verifiedSerial != serial ||
      std::memcmp(&p, &verified, sizeof(p))) {
    LOG_ERR("BTEXT", "Could not save book text preferences");
    return false;
  }
  saved = p;
  haveSaved = true;
  activeSlot = 1 - activeSlot;
  sequence = serial;
  return true;
}
}  // namespace
bool begin(const std::string& book) {
  if (active) {
    LOG_ERR("BTEXT", "Reader text scope already active");
    return false;
  }
  defaults = capture();
  std::snprintf(directory, sizeof(directory), "/.crosspoint/epub_%llu",
                static_cast<unsigned long long>(std::hash<std::string>{}(book)));
  active = true;
  haveSaved = false;
  activeSlot = 1;
  sequence = 0;
  for (unsigned slot = 0; slot < 2; ++slot) {
    Preferences p;
    uint32_t serial = 0;
    if (read(slot, p, serial) && (!haveSaved || int32_t(serial - sequence) > 0)) {
      saved = p;
      sequence = serial;
      activeSlot = slot;
      haveSaved = true;
    }
  }
  if (haveSaved && SETTINGS.perBookTextSettings) apply(saved);
  return true;
}
bool save() {
  if (!active) return true;
  const auto p = capture();
  if (!SETTINGS.perBookTextSettings) {
    if (!std::memcmp(&p, &defaults, sizeof(p))) return true;
    const auto previous = defaults;
    defaults = p;
    if (SETTINGS.saveToFile()) return true;
    defaults = previous;
    LOG_ERR("BTEXT", "Could not save shared text preferences");
    return false;
  }
  if (!std::memcmp(&p, haveSaved ? &saved : &defaults, sizeof(p))) return true;
  return writeBook(p);
}
bool isActive() { return active; }
bool copyAsDefaults() {
  const auto current = capture();
  if (!valid(current) || (active && !save())) return false;
  const auto previous = defaults;
  if (active) defaults = current;
  if (SETTINGS.saveToFile()) return true;
  defaults = previous;
  LOG_ERR("BTEXT", "Could not save reading defaults");
  return false;
}
bool setPerBook(bool enabled) {
  if (enabled == (SETTINGS.perBookTextSettings != 0)) return true;
  if (active && !save()) return false;
  const auto previous = defaults;
  const auto oldMode = SETTINGS.perBookTextSettings;
  defaults = capture();
  SETTINGS.perBookTextSettings = enabled;
  if (active && enabled && !writeBook(defaults)) {
    SETTINGS.perBookTextSettings = oldMode;
    defaults = previous;
    return false;
  }
  if (SETTINGS.saveToFile()) return true;
  SETTINGS.perBookTextSettings = oldMode;
  defaults = previous;
  LOG_ERR("BTEXT", "Could not save text preference mode");
  return false;
}
void end() {
  if (!active) return;
  save();
  apply(defaults);
  active = false;
}
void persist() {
  if (active)
    save();
  else
    SETTINGS.saveToFile();
}
void writeGlobalDefaults(JsonDocument& doc) {
  if (!active || !SETTINGS.perBookTextSettings) return;
#define WRITE(member, key) doc[key] = defaults.values[member];
  TEXT_FIELDS(WRITE)
#undef WRITE
  doc["sdFontFamilyName"] = defaults.family;
}
#undef TEXT_FIELDS
}  // namespace booktext
