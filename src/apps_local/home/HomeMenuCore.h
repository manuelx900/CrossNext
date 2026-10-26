#pragma once

#include <cstddef>
#include <cstdint>

namespace homemenu {
enum class Id : uint8_t {
  Files,
  Recent,
  Stats,
  Settings,
  Games,
  Apps,
  Transfer,
  Services,
  Shortcut0,
  Shortcut1,
  Shortcut2,
  Shortcut3,
  Shortcut4,
  Count
};
constexpr int kCount = static_cast<int>(Id::Count);
constexpr int kBaseCount = static_cast<int>(Id::Shortcut0);
constexpr int kShortcutCount = kCount - kBaseCount;
constexpr int kMaxVisible = 8;
constexpr char kSaveVersion = '4';
constexpr size_t kLabelBytes = 49;
constexpr size_t kItemBytes = 64;
constexpr size_t kLegacySaveBytes = 416;
constexpr size_t kSaveBytes = 992;
struct Shortcut {
  uint8_t folder = 0;
  char title[kItemBytes]{};
};
struct Config {
  uint16_t visible = 0x7f;
  char labels[kCount][kLabelBytes]{};
  Shortcut shortcuts[kShortcutCount]{};
  Id order[kCount] = {Id::Files,     Id::Recent,    Id::Transfer,  Id::Stats,     Id::Games,
                      Id::Apps,      Id::Services,  Id::Shortcut0, Id::Shortcut1, Id::Shortcut2,
                      Id::Shortcut3, Id::Shortcut4, Id::Settings};
};
bool isShortcut(Id id);
const Shortcut* shortcut(const Config& config, Id id);
Id findShortcut(const Config& config, int folder, const char* title);
Id addShortcut(Config& config, int folder, const char* title);
void removeShortcut(Config& config, Id id);
int visibleCount(const Config& config, bool services = true);
Id at(const Config& config, int index, bool services = true);
int indexOf(const Config& config, Id id, bool services = true);
bool move(Config& config, Id id, int direction, bool services = true);
bool setVisible(Config& config, Id id, bool shown);
bool setLabel(Config& config, Id id, const char* label);
uint32_t checksum(const uint8_t* data, size_t length);
void encode(const Config& config, uint32_t generation, uint8_t* bytes);
bool decode(const uint8_t* bytes, Config& config, uint32_t& generation, size_t length = kSaveBytes);
}  // namespace homemenu
