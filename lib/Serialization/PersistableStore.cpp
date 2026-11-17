#include "PersistableStore.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <algorithm>
#include <cstring>
#include <limits>

bool PersistableStoreBase::writeDocToFile(const char* path, const JsonDocument& doc, const bool onlyIfChanged) {
  String json;
  serializeJson(doc, json);
  if (onlyIfChanged && (doc.overflowed() || json.length() != measureJson(doc))) {
    LOG_ERR("PERSIST", "Failed to serialize %s", path);
    return false;
  }
  if (onlyIfChanged) {
    HalFile file;
    if (Storage.openFileForRead("PERSIST", path, file) && file.fileSize() == json.length()) {
      char bytes[128];
      size_t at = 0;
      while (at < json.length()) {
        const size_t count = std::min(sizeof(bytes), json.length() - at);
        if (file.read(bytes, count) != static_cast<int>(count) || std::memcmp(bytes, json.c_str() + at, count)) break;
        at += count;
      }
      if (at == json.length()) return true;
    }
  }
  Storage.mkdir("/.crosspoint");
  if (!Storage.writeFile(path, json)) {
    LOG_ERR("PERSIST", "Failed to write %s", path);
    return false;
  }
  return true;
}

bool PersistableStoreBase::readDocFromFile(const char* path, JsonDocument& doc) {
  if (!Storage.exists(path)) {
    return false;  // Expected on first boot — not an error.
  }
  String json = Storage.readFile(path);
  if (json.isEmpty()) {
    LOG_ERR("PERSIST", "Failed to read %s (empty)", path);
    return false;
  }
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("PERSIST", "JSON parse error in %s: %s", path, error.c_str());
    return false;
  }
  return true;
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave) {
  bool valid = false;
  return extractPassword(doc, needsResave, std::numeric_limits<size_t>::max(), valid);
}

std::string PersistableStoreBase::extractPassword(JsonVariantConst doc, bool& needsResave, const size_t maxLength,
                                                  bool& valid) {
  valid = true;
  bool ok = false;
  bool tooLong = false;
  std::string pass = obfuscation::deobfuscateFromBase64(doc["password_obf"] | "", maxLength, &ok, &tooLong);
  if (tooLong) {
    valid = false;
    return "";
  }
  if (!ok) {
    // Deobfuscation failed — fall back to legacy plaintext password.
    const char* legacyPassword = doc["password"] | "";
    const size_t legacyLength = strlen(legacyPassword);
    if (legacyLength > maxLength) {
      valid = false;
      return "";
    }
    pass.assign(legacyPassword, legacyLength);
    if (!pass.empty()) needsResave = true;
  }
  // A successfully decoded empty string is a legitimate value; preserve as-is.
  return pass;
}
