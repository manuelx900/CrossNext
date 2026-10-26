#pragma once

#include <string>

#include "Epub.h"

class Xtc {
  std::string path;

 public:
  Xtc(const std::string& path, const char*) : path(path) {}
  bool load() {
    ++fake::parses;
    return bookMetadata[path].success;
  }
  std::string getTitle() const { return bookMetadata[path].title; }
  std::string getAuthor() const { return bookMetadata[path].author; }
};
