#pragma once

#include <HalMemory.h>
#include <HalStorage.h>

// Build-local write coalescing; every completed page is drained before publication.
class PageCacheWriter final : public Print {
 public:
  static constexpr size_t BUFFER_SIZE = 4096;
  explicit PageCacheWriter(HalFile& file);
  using Print::write;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t* data, size_t size) override;
  bool drain();

 private:
  HalFile& file;
  HalMemory::PsramBuffer buffer;
  size_t used = 0;
  bool failed = false;
};
