#pragma once
#include <Epub.h>
class Xtc {
  std::string path;

 public:
  explicit Xtc(const char* p, const char*, bool) : path(p) {}
  const std::string& getPath() const { return path; }
  std::string getCachePath() const { return "/.crosspoint/test-xtc" + path; }
  std::string getThumbBmpPath(int h) const { return getCachePath() + "/thumb_" + std::to_string(h) + ".bmp"; }
  bool load() {
    ++xtcsLoaded;
    return true;
  }
  void setupCacheDir() {}
  bool generateThumbBmp(int height) {
    ++thumbs;
    if (path.find("thumb-error") != std::string::npos) return false;
    testStorage.files[getThumbBmpPath(height)] = {1};
    return true;
  }
};
