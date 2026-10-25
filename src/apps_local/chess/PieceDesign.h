#pragma once
#include <cstdint>
#include <cstdio>

namespace chessstyle {
enum class Design : uint8_t { Pixel, Classic, Ornate, Count };
inline constexpr Design kDefault = Design::Classic;
inline constexpr char kSettingsPath[] = "/.crosspoint/chess.cfg";
constexpr Design fromStored(int) { return kDefault; }
inline Design parsePreference(const char*) { return kDefault; }
Design loadPreference();
}  // namespace chessstyle
