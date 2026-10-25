#include <Zip.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

namespace {
size_t allocationLimit = std::numeric_limits<size_t>::max();
size_t largestAllocation = 0;
}

// Model the largest contiguous block on a constrained device.
void* operator new[](size_t size, const std::nothrow_t&) noexcept {
  if (size > largestAllocation) largestAllocation = size;
  return size <= allocationLimit ? std::malloc(size) : nullptr;
}
void operator delete[](void* ptr) noexcept { std::free(ptr); }

struct Source : freeink::content::ByteSource {
  std::vector<uint8_t> bytes;
  bool shortRead = false;
  uint64_t size() const override { return bytes.size(); }
  int32_t readAt(uint64_t offset, void* dst, uint32_t len) override {
    if (offset > bytes.size() || len > bytes.size() - offset) return -1;
    if (shortRead && len > 0) --len;
    std::memcpy(dst, bytes.data() + offset, len);
    return static_cast<int32_t>(len);
  }
};

void put16(std::vector<uint8_t>& bytes, size_t at, uint16_t value) {
  bytes[at] = value & 0xff;
  bytes[at + 1] = value >> 8;
}

Source directory(uint16_t count) {
  Source source;
  source.bytes.reserve(static_cast<size_t>(count) * 55 + 22);
  for (uint16_t i = 0; i < count; ++i) {
    char name[10];
    std::snprintf(name, sizeof(name), "file%05u", i);
    const size_t at = source.bytes.size();
    source.bytes.resize(at + 55);
    source.bytes[at] = 0x50;
    source.bytes[at + 1] = 0x4b;
    source.bytes[at + 2] = 0x01;
    source.bytes[at + 3] = 0x02;
    put16(source.bytes, at + 28, 9);
    std::memcpy(source.bytes.data() + at + 46, name, 9);
  }
  const size_t at = source.bytes.size();
  source.bytes.resize(at + 22);
  source.bytes[at] = 0x50;
  source.bytes[at + 1] = 0x4b;
  source.bytes[at + 2] = 0x05;
  source.bytes[at + 3] = 0x06;
  put16(source.bytes, at + 10, count);
  return source;
}

int main() {
  freeink::content::ZipScan scan;
  auto large = directory(9001);
  assert(scan.open(large));
  assert(scan.find("file00000") && scan.find("file09000"));
  assert(!scan.find("META-INF/encryption.xml"));
  assert(largestAllocation == 9001 * sizeof(freeink::content::ZipEntryInfo));

  allocationLimit = 64 * 1024;
  assert(!scan.open(large));
  assert(!scan.find("file00000"));

  auto small = directory(2);
  assert(scan.open(small));
  assert(scan.find("file00001"));
  allocationLimit = 0;
  assert(!scan.open(small));
  allocationLimit = 64 * 1024;
  small.shortRead = true;
  assert(!scan.open(small));
  auto empty = directory(0);
  assert(scan.open(empty) && !scan.find("anything"));
  puts("zip: all tests passed");
}
