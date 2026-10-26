#pragma once
#include <Epub.h>

#include <memory>
class ChapterXPathResolver {
 public:
  static std::string findXPathForProgress(const std::shared_ptr<Epub>&, int spine, float intra) {
    return "/body/DocFragment[" + std::to_string(spine + 1) + "]/body/p[1]/text()." +
           std::to_string(int(intra * 10000));
  }
};
