#pragma once

#include <ArduinoJson.h>

#include "ReaderBarsConfig.h"

namespace readerbars {
inline void writeConfig(JsonObject out, const Config& config) {
  out["visible"] = config.visible;
  out["progress"] = static_cast<uint8_t>(config.progress);
  out["thickness"] = config.thickness;
  auto slots = out["slots"].to<JsonArray>();
  for (const auto item : config.slots) slots.add(static_cast<uint8_t>(item));
}
inline void writeJson(JsonDocument& doc, const Settings& settings) {
  if (!settings.customized) return;
  auto out = doc["readerBars"].to<JsonObject>();
  out["version"] = 1;
  writeConfig(out["top"].to<JsonObject>(), settings.top);
  writeConfig(out["bottom"].to<JsonObject>(), settings.bottom);
}
inline bool readConfig(JsonVariantConst in, Config& config) {
  if (!in.is<JsonObjectConst>() || !in["visible"].is<bool>() || !in["progress"].is<uint8_t>() ||
      in["progress"].as<unsigned>() > 2 || !in["thickness"].is<uint8_t>() || in["thickness"].as<unsigned>() > 2)
    return false;
  const auto slots = in["slots"].as<JsonArrayConst>();
  if (slots.size() != SLOT_COUNT) return false;
  for (unsigned i = 0; i < SLOT_COUNT; ++i) {
    if (!slots[i].is<uint8_t>() || slots[i].as<unsigned>() >= static_cast<unsigned>(Item::Count)) return false;
    config.slots[i] = static_cast<Item>(slots[i].as<uint8_t>());
  }
  config.visible = in["visible"].as<bool>();
  config.progress = static_cast<Progress>(in["progress"].as<uint8_t>());
  config.thickness = in["thickness"].as<uint8_t>();
  return true;
}
inline bool readJson(JsonVariantConst doc, Settings& settings) {
  const auto in = doc["readerBars"];
  if (in.isNull()) return true;
  Settings candidate;
  if (!in["version"].is<uint8_t>() || in["version"].as<unsigned>() != 1 || !readConfig(in["top"], candidate.top) ||
      !readConfig(in["bottom"], candidate.bottom))
    return false;
  candidate.customized = true;
  settings = candidate;
  return true;
}
}  // namespace readerbars
