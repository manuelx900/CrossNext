#pragma once
#include <Epub.h>

#include <memory>
#include <optional>
class Section {
 public:
  Section(const std::shared_ptr<Epub>&, int, GfxRenderer&) {}
  std::optional<int> getCachedPageCount() const { return 100; }
};
