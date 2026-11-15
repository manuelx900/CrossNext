#pragma once
#include <HalStorage.h>

#include <map>
#include <set>
#include <string>
inline unsigned epubsLoaded = 0, xtcsLoaded = 0, builds = 0, thumbs = 0;
inline std::set<std::string> prepared;
class Epub {
 public:
  std::string path;
  explicit Epub(std::string p, const char*) : path(p) {}
  const std::string& getPath() const { return path; }
  std::string getCachePath() const { return "/.crosspoint/test-book" + path; }
  std::string getThumbBmpPath(int h) const { return getCachePath() + "/thumb_" + std::to_string(h) + ".bmp"; }
  bool extractItemToFile(const char*, const char*) { return true; }
  bool load(bool, bool) {
    ++epubsLoaded;
    return path.find("bad") == std::string::npos;
  }
  int getSpineItemsCount() { return 3; }
  bool hasSupportedThumbnailSource() const { return path.find("no-cover") == std::string::npos; }
  bool generateThumbBmp(int height) {
    ++thumbs;
    if (path.find("thumb-error") != std::string::npos) return false;
    testStorage.files[getThumbBmpPath(height)] = {1};
    return true;
  }
};
