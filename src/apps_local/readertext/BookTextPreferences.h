#pragma once
#include <ArduinoJson.h>

#include <string>

namespace booktext {
// One reader owns the effective text settings. Private mode preserves global
// defaults during serialization; shared mode persists edits as global defaults.
bool begin(const std::string& book);
bool isActive();
bool save();
bool copyAsDefaults();
bool setPerBook(bool enabled);
void persist();
void end();
void writeGlobalDefaults(JsonDocument& doc);
}  // namespace booktext
