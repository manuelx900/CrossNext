#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "apps_local/home/CoverBitmapPainter.h"
#include "apps_local/library/LibraryCoverPrefetch.h"

namespace {
unsigned checks = 0, failures = 0;
void check(bool condition) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("failed check %u\n", checks);
  }
}
std::vector<uint8_t> bmp(int width, int height, unsigned depth, bool topDown) {
  const unsigned offset = 54 + (1u << depth) * 4, stride = (width * depth + 31) / 32 * 4;
  std::vector<uint8_t> bytes(offset + stride * height);
  const auto put = [&](unsigned at, uint32_t value, unsigned size) {
    for (unsigned i = 0; i < size; ++i) bytes[at + i] = value >> (8 * i);
  };
  put(0, 0x4d42, 2);
  put(2, bytes.size(), 4);
  put(10, offset, 4);
  put(14, 40, 4);
  put(18, width, 4);
  put(22, topDown ? -height : height, 4);
  put(26, 1, 2);
  put(28, depth, 2);
  for (unsigned i = 0; i < (1u << depth); ++i) put(54 + i * 4, i * 255 / ((1u << depth) - 1) * 0x010101, 4);
  for (size_t i = offset; i < bytes.size(); ++i) bytes[i] = (i * 71 + i / stride * 19) & 255;
  return bytes;
}
using Prefetch = libraryui::CoverPrefetch;
Prefetch::Result load(Prefetch& cache, size_t slot, int height) {
  Prefetch::Result result;
  unsigned chunks = 0;
  do {
    const auto before = testStorage.readBytes;
    result = cache.load(slot, height, "/cover.bmp");
    check(testStorage.readBytes - before <= Prefetch::CHUNK_BYTES + 54);
    check(++chunks <= Prefetch::CAPACITY / Prefetch::CHUNK_BYTES + 1);
  } while (result == Prefetch::Result::Pending);
  return result;
}
}  // namespace

int main(int argc, char**) {
  HalMemory::fail = argc > 1;
  Prefetch cache;
  cache.begin(10, 20, 30);
  testStorage.files["/cover.bmp"] = bmp(200, 300, 1, false);
  if (HalMemory::fail) {
    for (unsigned i = 0; i < 30; ++i) check(cache.load(0, 300, "/cover.bmp") == Prefetch::Result::Unavailable);
    check(HalMemory::allocations == 1);
    check(testStorage.readBytes == 0);
    check(!cache.find(10, 20, 30, 0, 300));
  } else {
    GfxRenderer renderer;
    unsigned cases = 0;
    uint64_t foregroundSdBytes = 0;
    for (bool landscape : {false, true})
      for (unsigned depth : {1u, 2u})
        for (bool topDown : {false, true})
          for (int width : {64, 240, 640})
            for (int height : {96, 300}) {
              renderer.width = landscape ? 800 : 480;
              renderer.height = landscape ? 480 : 800;
              testStorage.files["/cover.bmp"] = bmp(width, height, depth, topDown);
              cache.begin(++cases + 100, 20, 30);
              const auto allocations = HalMemory::allocations;
              check(load(cache, 0, height) == Prefetch::Result::Ready);
              check(cases == 1 || HalMemory::allocations == allocations);
              const auto bytes = cache.find(cases + 100, 20, 30, 0, height);
              check(bool(bytes));
              check(bytes.size == testStorage.files["/cover.bmp"].size());
              check(!std::memcmp(bytes.data, testStorage.files["/cover.bmp"].data(), bytes.size));
              const auto slot = freeink::ui::makeRect(13, 17, landscape ? 173 : 113, landscape ? 199 : 177);
              HalFile file;
              check(Storage.openFileForRead("TEST", "/cover.bmp", file));
              Bitmap bitmap(file);
              check(bitmap.parseHeaders() == BmpReaderError::Ok);
              std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
              auto before = testStorage.readBytes;
              check(homecover::paintBitmap(renderer, bitmap, slot));
              foregroundSdBytes += testStorage.readBytes - before;
              const auto reference = renderer.frame;
              file.close();
              bitmap.setMemorySource(bytes.data, bytes.size);
              before = testStorage.readBytes;
              testStorage.refuseReads = true;
              check(bitmap.parseHeaders() == BmpReaderError::Ok);
              std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
              check(homecover::paintBitmap(renderer, bitmap, slot));
              check(renderer.frame == reference);
              check(testStorage.readBytes == before);
              check(bitmap.rewindToData() == BmpReaderError::Ok);
              bitmap.setMemorySource(nullptr, 0);
              testStorage.refuseReads = false;
              check(bitmap.parseHeaders() == BmpReaderError::FileInvalid);
            }
    std::printf("Cover parity: %u cases; foreground SD bytes %llu -> 0 (same pixels)\n", cases,
                static_cast<unsigned long long>(foregroundSdBytes));
    cache.begin(11, 20, 30);
    testStorage.files["/cover.bmp"] = bmp(200, 300, 1, false);
    check(cache.load(0, 300, "/cover.bmp") == Prefetch::Result::Pending);
    check(!cache.find(11, 20, 30, 0, 300));
    cache.cancelLoad();
    check(load(cache, 0, 300) == Prefetch::Result::Ready);
    check(!cache.find(12, 20, 30, 0, 300));
    check(!cache.find(11, 21, 30, 0, 300));
    check(!cache.find(11, 20, 31, 0, 300));
    check(!cache.find(11, 20, 30, 1, 300));
    check(!cache.find(11, 20, 30, 0, 301));
    cache.begin(11, 20, 30);  // same page preserves completed covers
    check(bool(cache.find(11, 20, 30, 0, 300)));
    cache.begin(12, 20, 30);
    testStorage.readLimit = 10;
    check(cache.load(0, 300, "/cover.bmp") == Prefetch::Result::Unavailable);
    check(!cache.find(12, 20, 30, 0, 300));
    testStorage.readLimit = -1;
    check(cache.load(0, 300, "/absent.bmp") == Prefetch::Result::Unavailable);
    testStorage.files["/cover.bmp"].resize(Prefetch::CAPACITY + 1);
    for (unsigned i = 0; i < 4; ++i) testStorage.files["/cover.bmp"][2 + i] = (Prefetch::CAPACITY + 1) >> (8 * i);
    auto before = testStorage.readBytes;
    check(cache.load(0, 300, "/cover.bmp") == Prefetch::Result::Unavailable);
    check(testStorage.readBytes - before <= 54);
    testStorage.files["/cover.bmp"].resize(32768);
    for (unsigned i = 0; i < 4; ++i) testStorage.files["/cover.bmp"][2 + i] = 32768u >> (8 * i);
    for (unsigned n = 0; n < 4; ++n) check(load(cache, n, 300) == Prefetch::Result::Ready);
    check(cache.bytesUsed() == Prefetch::CAPACITY);
    before = testStorage.readBytes;
    check(cache.load(4, 300, "/cover.bmp") == Prefetch::Result::Unavailable);
    check(testStorage.readBytes - before <= 54);
    cache.release();
    check(!cache.find(12, 20, 30, 0, 300));
    const auto allocations = HalMemory::allocations;
    HalMemory::freeBytes = Prefetch::CAPACITY;
    cache.begin(15, 20, 30);
    check(cache.load(0, 300, "/cover.bmp") == Prefetch::Result::Unavailable);
    check(!cache.enabled());
    check(HalMemory::allocations == allocations);
    cache.release();
    HalMemory::freeBytes = 8 * 1024 * 1024;
    HalMemory::largestBlockBytes = Prefetch::CAPACITY - 1;
    cache.begin(16, 20, 30);
    check(cache.load(0, 300, "/cover.bmp") == Prefetch::Result::Unavailable);
    check(!cache.enabled());
    check(HalMemory::allocations == allocations);
  }
  std::printf("Library prefetch: %u checks, %u failures\n", checks, failures);
  return failures ? 1 : 0;
}
