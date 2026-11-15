#pragma once

#include <cstdint>
#include <string_view>

#include "EpubCachePath.h"

enum class StorageMutation : uint8_t { Resources, ReadingState, DerivedCache };

struct StorageStateKey {
  enum class Kind : uint8_t { None, EpubProgress, XtcProgress, ReadingJournal };
  uint64_t book = 0;
  Kind kind = Kind::None;
  bool valid() const { return kind != Kind::None; }
};

inline StorageStateKey storageStateKey(const char* path) {
  if (!path) return {};
  std::string_view value(path);
  if (value == "/.crosspoint/reading-history/sessions.bin") return {0, StorageStateKey::Kind::ReadingJournal};
  constexpr std::string_view root = "/.crosspoint/";
  if (value.substr(0, root.size()) != root) return {};
  value.remove_prefix(root.size());
  const auto slash = value.find('/');
  if (slash == std::string_view::npos) return {};
  const auto name = value.substr(0, slash);
  const bool epub = name.substr(0, 5) == "epub_";
  const size_t prefix = epub ? 5 : name.substr(0, 4) == "xtc_" ? 4 : 0;
  if (!prefix || name.size() <= prefix || (name.size() > prefix + 1 && name[prefix] == '0')) return {};
  uint64_t number = 0;
  for (size_t n = prefix; n < name.size(); ++n) {
    if (name[n] < '0' || name[n] > '9') return {};
    const unsigned digit = unsigned(name[n] - '0');
    if (number > (UINT64_MAX - digit) / 10) return {};
    number = number * 10 + digit;
  }
  const auto file = value.substr(slash + 1);
  if (file != "progress.bin" && file != "progress.bin.tmp") return {};
  return {number, epub ? StorageStateKey::Kind::EpubProgress : StorageStateKey::Kind::XtcProgress};
}

inline bool isReadingIndex(const char* path) {
  if (!path) return false;
  const std::string_view value(path), root = "/.crosspoint/reading-history/";
  if (value.size() != root.size() + 36 || value.substr(0, root.size()) != root ||
      value.substr(value.size() - 4) != ".idx")
    return false;
  for (size_t n = root.size(); n < root.size() + 32; ++n)
    if (!((value[n] >= '0' && value[n] <= '9') || (value[n] >= 'a' && value[n] <= 'f'))) return false;
  return true;
}

inline bool isLibraryScenePack(const char* path) {
  if (!path) return false;
  const std::string_view value(path), prefix = "/.crosspoint/library-grid-";
  return value.size() == prefix.size() + 6 && value.substr(0, prefix.size()) == prefix && value[prefix.size()] >= '0' &&
         value[prefix.size()] < '8' && value.substr(prefix.size() + 1) == ".pack";
}

inline bool isPreparedCover(const char* path) {
  if (!path) return false;
  std::string_view value(path), root = "/.crosspoint/";
  if (value.substr(0, root.size()) != root) return false;
  value.remove_prefix(root.size());
  const auto slash = value.find('/');
  if (slash == std::string_view::npos) return false;
  const auto directory = value.substr(0, slash);
  const size_t prefix = directory.substr(0, 5) == "epub_" ? 5 : directory.substr(0, 4) == "xtc_" ? 4 : 0;
  if (!prefix || directory.size() == prefix) return false;
  for (size_t i = prefix; i < directory.size(); ++i)
    if (directory[i] < '0' || directory[i] > '9') return false;
  value.remove_prefix(slash + 1);
  if (value.substr(0, 6) != "thumb_") return false;
  value.remove_prefix(6);
  if (value.substr(0, 5) == "ccw3_") value.remove_prefix(5);
  if (value.size() <= 4 || value.substr(value.size() - 4) != ".bmp") return false;
  for (size_t i = 0; i < value.size() - 4; ++i)
    if (value[i] < '0' || value[i] > '9') return false;
  return true;
}

// Only the reserved progress files in a canonical book-cache directory qualify.
inline StorageMutation storageMutation(const char* path) {
  if (!path) return StorageMutation::Resources;
  std::string_view value(path);
  if (value == "/.crosspoint/state.json" || storageStateKey(path).valid() || isBookTextPreferences(path))
    return StorageMutation::ReadingState;
  constexpr std::string_view root = "/.crosspoint/";
  if (value.substr(0, root.size()) != root) return StorageMutation::Resources;
  value.remove_prefix(root.size());
  const size_t slash = value.find('/');
  if (slash == std::string_view::npos) return StorageMutation::Resources;
  const auto name = value.substr(0, slash);
  const size_t prefix = name.substr(0, 5) == "epub_" ? 5 : name.substr(0, 4) == "xtc_" ? 4 : 0;
  if (!prefix || name.size() == prefix) return StorageMutation::Resources;
  for (size_t i = prefix; i < name.size(); ++i)
    if (name[i] < '0' || name[i] > '9') return StorageMutation::Resources;
  const auto file = value.substr(slash + 1);
  return file == "progress.bin" || file == "progress.bin.tmp" ? StorageMutation::ReadingState
                                                              : StorageMutation::Resources;
}

inline StorageMutation storageMutation(const char* from, const char* to) {
  return storageMutation(from) == StorageMutation::ReadingState && storageMutation(to) == StorageMutation::ReadingState
             ? StorageMutation::ReadingState
             : StorageMutation::Resources;
}
