#pragma once

#include <map>
#include <optional>
#include <string>

#include "HalStorage.h"

struct FakeMetadata {
  std::string title = "Title";
  std::string author = "Author";
  bool success = true;
  std::string series{}, genre{};
  std::optional<float> seriesIndex{};
};

inline std::map<std::string, FakeMetadata> bookMetadata;

class Epub {
  std::string path;

 public:
  Epub(const std::string& path, const char*) : path(path) {}

  struct SyncMetadata {
    std::string title, author, series, genre;
    std::optional<float> seriesIndex;
  };
  bool loadMetadata(std::string& title, std::string& author, SyncMetadata* details = nullptr) {
    ++fake::parses;
    const auto& metadata = bookMetadata[path];
    if (!metadata.success) return false;
    title = metadata.title;
    author = metadata.author;
    if (details) *details = {title, author, metadata.series, metadata.genre, metadata.seriesIndex};
    return true;
  }
};
