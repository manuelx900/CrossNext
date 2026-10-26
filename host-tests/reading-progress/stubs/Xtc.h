#pragma once
#include <Epub.h>
class Xtc {
  std::string path;

 public:
  Xtc(const std::string& path, const std::string&) : path(path) {}
  bool load() const { return Storage.exists(path.c_str()); }
  uint32_t getPageCount() const { return 100; }
  std::string getTitle() const { return path; }
  std::string getAuthor() const { return "Artist"; }
};
