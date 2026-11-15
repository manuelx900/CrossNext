#pragma once
#include <functional>
#include <string>
inline unsigned testEpubLoads = 0;
class Epub {
 public:
  explicit Epub(const std::string& path, const char*)
      : cache("/.crosspoint/epub_" + std::to_string(std::hash<std::string>{}(path))) {}
  bool load(bool, bool) {
    ++testEpubLoads;
    return true;
  }
  const std::string& getCachePath() const { return cache; }
  int getSpineItemsCount() const { return 2; }
  unsigned getBookSize() const { return 1000; }
  float calculateProgress(int spine, float fraction) const { return (spine + fraction) / 2; }

 private:
  std::string cache;
};
