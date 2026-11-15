#pragma once

#include <Epub/Page.h>
#include <Epub/ReaderRenderSpec.h>

namespace readerpage {
struct Key {
  const void* section = nullptr;
  int spine = -1, page = -1;
  ReaderRenderSpec spec;
  uint32_t resource = 0, media = 0;
  bool operator==(const Key&) const = default;
  bool valid() const { return section && spine >= 0 && page >= 0 && resource && media; }
};

// Retains the already-loaded graph, never a second framebuffer or a copy.
class Reuse {
 public:
  static constexpr size_t Budget = 64 * 1024;
  bool matches(const Key& wanted) const { return page && wanted.valid() && wanted == key; }
  std::unique_ptr<Page> take(const Key& wanted) {
    if (!matches(wanted)) clear();
    bytes = 0;
    return std::move(page);
  }
  bool keep(std::unique_ptr<Page>& value, const Key& source, size_t otherBytes) {
    clear();
    if (!value || !source.valid() || otherBytes > Budget) return false;
    const size_t size = value->retainedBytes();
    if (size > Budget - otherBytes) return false;
    bytes = size;
    key = source;
    page = std::move(value);
    return true;
  }
  void clear() {
    page.reset();
    bytes = 0;
    key = {};
  }
  size_t retainedBytes() const { return bytes; }

 private:
  std::unique_ptr<Page> page;
  Key key;
  size_t bytes = 0;
};
}  // namespace readerpage
