#pragma once

#include <cstdint>
#include <string_view>

#include "EpubCachePath.h"

enum class StorageMutation : uint8_t { Resources, ReadingState, DerivedCache, NavigationState };

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

inline bool isBookOrbitPendingFile(std::string_view value) {
  constexpr std::string_view root = "/.crosspoint/bookorbit-pending";
  if (value == root) return true;
  if (!value.starts_with(root) || value.size() <= root.size() || value[root.size()] != '/') return false;
  value.remove_prefix(root.size() + 1);
  if (value.size() <= 16 || (value.substr(16) != ".pending" && value.substr(16) != ".tmp")) return false;
  for (size_t i = 0; i < 16; ++i) {
    const char c = value[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  }
  return true;
}

inline bool isVocabularyNotice(const char* path) {
  if (!path) return false;
  std::string_view value(path);
  if (value == "/.crosspoint/vocabulary/.notice-current.tmp" || value == "/.crosspoint/vocabulary/.notice-accepted.bin")
    return true;
  constexpr std::string_view prefix = "/.crosspoint/vocabulary/acknowledged/";
  if (value.size() != prefix.size() + 39 || value.substr(0, prefix.size()) != prefix) return false;
  value.remove_prefix(prefix.size());
  if (value[2] != '/' || value[0] != value[3] || value[1] != value[4] || value.substr(35) != ".ack") return false;
  for (size_t n = 3; n < 35; ++n)
    if (!((value[n] >= '0' && value[n] <= '9') || (value[n] >= 'a' && value[n] <= 'f'))) return false;
  return true;
}
inline bool isVocabularyRecord(const char* path) {
  if (!path) return false;
  std::string_view value(path);
  constexpr std::string_view words = "/.crosspoint/vocabulary/words/", contexts = "/.crosspoint/vocabulary/contexts/";
  const auto prefix = value.substr(0, words.size()) == words ? words : contexts;
  if (value.size() != prefix.size() + 36 || value.substr(0, prefix.size()) != prefix) return false;
  value.remove_prefix(prefix.size());
  for (size_t n = 0; n < 32; ++n)
    if (!((value[n] >= '0' && value[n] <= '9') || (value[n] >= 'a' && value[n] <= 'f'))) return false;
  const auto suffix = value.substr(32);
  return suffix == ".dat" || suffix == ".tmp" || suffix == ".bak";
}

// Navigation and reading state cannot alter book or cover resources.
inline StorageMutation storageMutation(const char* path) {
  if (!path) return StorageMutation::Resources;
  std::string_view value(path);
  if (value == "/.crosspoint/stats-period-a.tmp" || value == "/.crosspoint/stats-period-b.tmp" ||
      value == "/.crosspoint/stats-days-a.tmp" || value == "/.crosspoint/stats-days-b.tmp" ||
      value == "/.crosspoint/vocabulary/.list-words.tmp" || value == "/.crosspoint/vocabulary/.list-language.tmp")
    return StorageMutation::DerivedCache;
  if (value == "/.crosspoint/shelf.cfg" || value == "/.crosspoint/shelf-hidden.cfg")
    return StorageMutation::NavigationState;
  if (value == "/.crosspoint/state.json" || value == "/.crosspoint/reading-history/.reset.tmp" ||
      value == "/.crosspoint/reading-history/.reset.bak" || storageStateKey(path).valid() ||
      isBookTextPreferences(path) || isVocabularyRecord(path) || isVocabularyNotice(path) ||
      isBookOrbitPendingFile(value))
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
  const auto source = storageMutation(from), destination = storageMutation(to);
  if (source == destination && (source == StorageMutation::ReadingState || source == StorageMutation::NavigationState))
    return source;
  return StorageMutation::Resources;
}
