#pragma once

#include <HalMemory.h>
#include <HalStorage.h>

// Build-local coalescing, with optional PSRAM staging before the first format result.
class PageCacheWriter final : public Print {
 public:
  static constexpr size_t BUFFER_SIZE = 4096;
  explicit PageCacheWriter(HalFile& file);
  using Print::write;
  size_t write(uint8_t value) override;
  size_t write(const uint8_t* data, size_t size) override;
  bool drain();
  // Optional 256 KiB PSRAM page spool; unavailable memory keeps ordinary SD writes.
  bool deferPersistence();
  void allowPersistence();
  bool flushSome(size_t budget = 8192);
  bool flushAll();
  bool spoolNeedsDrain() const { return deferring && staged >= STAGING_BYTES / 2; }
  bool flushing() const { return staging && !deferring; }
  size_t position() const;
  const uint8_t* resident(size_t offset, size_t bytes) const;
  static constexpr size_t STAGING_BYTES = 256 * 1024;

 private:
  HalFile& file;
  HalMemory::PsramBuffer buffer;
  size_t used = 0;
  bool failed = false;
  HalMemory::PsramBuffer staging;
  size_t staged = 0, flushed = 0, stagingBase = 0;
  bool deferring = false;
};
