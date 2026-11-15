#include <HalStorage.h>
#define LOG_ERR(...) ((void)0)
#include <Serialization.h>

#include <cassert>
#include <cstdio>
#include <memory>
#include <optional>
struct Section {
  struct Entry {
    uint32_t visibleTextOffset;
  };
  struct Build {
    bool parserFinished = false;
    std::vector<Entry> lut;
  };
  std::unique_ptr<Build> build_ = std::make_unique<Build>();
  std::string filePath = "missing-format-layout.bin";
  static constexpr size_t HEADER_SIZE = 64;
  static constexpr uint8_t SECTION_FILE_VERSION = 13, SECTION_FILE_PARTIAL_VERSION = 14;
  std::optional<uint16_t> getPageForVisibleTextOffset(uint32_t, bool = false) const;
};
#include "format-offset-under-test.inc"
int main() {
  Section s;
  s.build_->lut = {{0}, {10}, {20}};
  assert(!s.getPageForVisibleTextOffset(27));
  s.build_->parserFinished = true;
  assert(s.getPageForVisibleTextOffset(27) == 2);
  assert(s.getPageForVisibleTextOffset(11) == 1);
  assert(s.getPageForVisibleTextOffset(0) == 0);
  s.build_->lut = {{0}, {10}, {10}, {10}};
  assert(s.getPageForVisibleTextOffset(10) == 3);
  assert(s.getPageForVisibleTextOffset(10, true) == 1);
  puts(
      "Production offset lookup: completed memory preview resolves final-page content and equal-offset ties without "
      "committed file");
}
