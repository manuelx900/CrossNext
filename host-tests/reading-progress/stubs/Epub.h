#pragma once
#include <HalStorage.h>

#include <string>
class GfxRenderer {};
class Epub {
  std::string path, cache;

 public:
  Epub(const std::string& path, const std::string& root)
      : path(path), cache(root + "/epub_" + std::to_string(std::hash<std::string>{}(path))) {}
  void setupCacheDir() const {}
  bool load(bool, bool) { return Storage.exists(path.c_str()); }
  const std::string& getCachePath() const { return cache; }
  const std::string& getTitle() const { return path; }
  std::string getAuthor() const { return "Author"; }
  size_t getBookSize() const { return 100000; }
  size_t getCumulativeSpineItemSize(int n) const { return (n + 1) * 10000; }
  int getSpineItemsCount() const { return 10; }
};
