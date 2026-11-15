#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <PackedRegion.h>

#include <cstdio>
#include <cstring>
#include <vector>

#include "apps_local/home/CoverBitmapPainter.h"
#include "apps_local/library/LibraryCoverPrefetch.h"
#include "apps_local/library/PreparedCoverCache.h"

namespace {
unsigned checks = 0, failures = 0;
void check(bool value) {
  ++checks;
  if (!value) {
    ++failures;
    std::printf("check %u failed\n", checks);
  }
}
std::vector<uint8_t> bmp(int w, int h, unsigned depth, bool top) {
  const unsigned offset = 54 + (1u << depth) * 4, stride = (w * depth + 31) / 32 * 4;
  std::vector<uint8_t> bytes(offset + stride * h);
  auto put = [&](unsigned at, uint32_t value, unsigned n) {
    for (unsigned i = 0; i < n; ++i) bytes[at + i] = value >> (8 * i);
  };
  put(0, 0x4d42, 2);
  put(2, bytes.size(), 4);
  put(10, offset, 4);
  put(14, 40, 4);
  put(18, w, 4);
  put(22, top ? -h : h, 4);
  put(26, 1, 2);
  put(28, depth, 2);
  for (unsigned i = 0; i < (1u << depth); ++i) put(54 + i * 4, i * 255 / ((1u << depth) - 1) * 0x010101, 4);
  for (size_t i = offset; i < bytes.size(); ++i) bytes[i] = (i * 71 + i / stride * 19) & 255;
  return bytes;
}
bool bit(const std::vector<uint8_t>& bytes, int x, int y, int stride) {
  return bytes[y * stride + x / 8] & (0x80 >> (x & 7));
}
void packedChecks() {
  constexpr int stride = 20, height = 8;
  std::vector<uint8_t> original(stride * height), canvas(stride * height);
  for (size_t n = 0; n < original.size(); ++n) original[n] = uint8_t(n * 71 + 33);
  for (int x = 0; x < 16; ++x)
    for (int width = 1; width <= 128; ++width) {
      std::vector<uint8_t> packed(packedregion::bytes(width, 3));
      packedregion::read(original.data(), stride, x, 2, width, 3, packed.data());
      for (int yy = 0; yy < 3; ++yy)
        for (int xx = 0; xx < width; ++xx)
          check(bit(packed, xx, yy, (width + 7) / 8) == bit(original, x + xx, 2 + yy, stride));
      std::fill(canvas.begin(), canvas.end(), 0x69);
      const auto outside = canvas;
      packedregion::write(canvas.data(), stride, x, 2, width, 3, packed.data());
      for (int yy = 0; yy < height; ++yy)
        for (int xx = 0; xx < stride * 8; ++xx)
          check(bit(canvas, xx, yy, stride) == (xx >= x && xx < x + width && yy >= 2 && yy < 5
                                                    ? bit(original, xx, yy, stride)
                                                    : bit(outside, xx, yy, stride)));
    }
}
void finish(libraryui::PreparedCoverCache& cache) {
  unsigned n = 0;
  while (cache.writing()) {
    const auto before = testStorage.readBytes;
    const auto writes = testStorage.writes;
    cache.step();
    check(testStorage.readBytes - before <= 4096);
    check(testStorage.writes - writes <= 1);
    check(++n < 128);
  }
}
bool cancel(void* state) { return ++*static_cast<unsigned*>(state) < 4; }
}  // namespace
int main(int argc, char**) {
  using Cache = libraryui::PreparedCoverCache;
  if (argc > 1) {
    HalMemory::fail = true;
    Cache cache;
    check(!cache.begin());
    check(!cache.begin());
    check(HalMemory::allocations == 1);
    std::printf("Native OOM: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
  }
  packedChecks();
  Cache cache;
  GfxRenderer renderer;
  unsigned cases = 0;
  for (auto orientation : {GfxRenderer::Portrait, GfxRenderer::LandscapeClockwise, GfxRenderer::PortraitInverted,
                           GfxRenderer::LandscapeCounterClockwise})
    for (bool inverted : {false, true})
      for (unsigned depth : {1u, 2u})
        for (int width : {64, 240, 640})
          for (bool top : {false, true}) {
            ++cases;
            renderer.orientation = orientation;
            renderer.inverted = inverted;
            const auto source = bmp(width, 240, depth, top);
            testStorage.files["/cover.bmp"] = source;
            HalFile file;
            check(Storage.openFileForRead("TEST", "/cover.bmp", file));
            Bitmap bitmap(file);
            auto area = freeink::ui::makeRect(13, 17, 113, 177);
            const auto key = Cache::key(renderer, area);
            std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
            const auto before = renderer.frame;
            check(cache.prepare(renderer, bitmap, area, key, "/cover.bmp"));
            file.close();
            check(renderer.frame == before);
            finish(cache);
            check(testStorage.files["/cover.bmp"].size() > source.size());
            check(!std::memcmp(testStorage.files["/cover.bmp"].data(), source.data(), source.size()));
            // Reuse at a different pixel alignment; no source scaling during the native draw.
            area.x = 78;
            area.y = 203;
            check(Cache::key(renderer, area) == key);
            check(Storage.openFileForRead("TEST", "/cover.bmp", file));
            check(bitmap.parseHeaders() == BmpReaderError::Ok);
            check(homecover::paintBitmap(renderer, bitmap, area));
            const auto reference = renderer.frame;
            std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
            renderer.fillCalls = 0;
            check(cache.load(file, renderer, area, key));
            check(renderer.frame == reference);
            check(renderer.fillCalls == 0);
            file.close();
            libraryui::CoverPrefetch prefetch;
            prefetch.begin(9, 10, 11);
            auto result = libraryui::CoverPrefetch::Result::Pending;
            unsigned jobs = 0;
            while (result == libraryui::CoverPrefetch::Result::Pending) {
              auto count = testStorage.readBytes;
              result = prefetch.load(0, 177, "/cover.bmp", key,
                                     renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height));
              // Header/footer probes are separate bounded metadata reads at file open.
              check(testStorage.readBytes - count <= 4096 + 54);
              check(++jobs < 128);
            }
            check(result == libraryui::CoverPrefetch::Result::Ready);
            auto bytes = prefetch.find(9, 10, 11, 0, 177, key);
            check(bool(bytes) && bytes.native);
            check(bytes.size == renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height));
            check(!prefetch.find(9, 10, 11, 0, 177, key + 1));
          }
  // A raw next-page preload becomes native immediately, including its first visit.
  {
    const auto area = freeink::ui::makeRect(11, 15, 113, 177);
    const auto key = Cache::key(renderer, area);
    testStorage.files["/first.bmp"] = bmp(240, 300, 1, false);
    libraryui::CoverPrefetch prefetch;
    prefetch.begin(21, 22, 23);
    auto result = libraryui::CoverPrefetch::Result::Pending;
    while (result == libraryui::CoverPrefetch::Result::Pending) result = prefetch.load(0, 177, "/first.bmp", key);
    check(result == libraryui::CoverPrefetch::Result::Ready);
    const auto raw = prefetch.find(21, 22, 23, 0, 177, key);
    check(raw && !raw.native);
    HalFile unused;
    Bitmap memory(unused);
    memory.setMemorySource(raw.data, raw.size);
    check(memory.parseHeaders() == BmpReaderError::Ok);
    const auto before = renderer.frame;
    check(homecover::paintBitmap(renderer, memory, area));
    const auto reference = renderer.frame;
    renderer.frame = before;
    check(cache.prepare(renderer, memory, area, key, "/first.bmp"));
    memory.setMemorySource(nullptr, 0);
    const auto size = renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height);
    check(prefetch.promoteLast(0, 177, key, cache.preparedPixels(), size));
    check(!prefetch.promoteLast(1, 177, key, cache.preparedPixels(), size));
    check(!prefetch.promoteLast(0, 178, key, cache.preparedPixels(), size));
    check(!prefetch.promoteLast(0, 177, key, cache.preparedPixels(), libraryui::CoverPrefetch::CAPACITY + 1));
    const auto native = prefetch.find(21, 22, 23, 0, 177, key);
    check(native && native.native && native.size == size);
    const auto reads = testStorage.readBytes;
    check(renderer.copyPackedBufferToRegion(area.x, area.y, area.width, area.height, native.data, native.size));
    renderer.preserveImagePolarity(area.x, area.y, area.width, area.height);
    check(renderer.frame == reference && testStorage.readBytes == reads);
    cache.cancel();
  }
  // Incomplete or modified caches never change the screen.
  const auto area = freeink::ui::makeRect(11, 15, 113, 177);
  const auto key = Cache::key(renderer, area);
  const auto source = bmp(240, 300, 1, false);
  testStorage.files["/cover.bmp"] = source;
  HalFile file;
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  Bitmap bitmap(file);
  check(cache.prepare(renderer, bitmap, area, key, "/cover.bmp"));
  file.close();
  finish(cache);
  auto valid = testStorage.files["/cover.bmp"];
  const auto frame = renderer.frame;
  testStorage.files["/cover.bmp"][source.size() + 3] ^= 1;
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  check(!cache.load(file, renderer, area, key));
  check(frame == renderer.frame);
  file.close();
  testStorage.files["/cover.bmp"] = valid;
  testStorage.files["/cover.bmp"][source.size() - 1] ^= 1;
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  check(!cache.load(file, renderer, area, key));
  check(frame == renderer.frame);
  file.close();
  testStorage.files["/cover.bmp"] = source;
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  unsigned calls = 0;
  check(!cache.prepare(renderer, bitmap, area, key, "/cover.bmp", cancel, &calls));
  check(frame == renderer.frame);
  check(testStorage.files["/cover.bmp"] == source);
  file.close();
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  check(cache.prepare(renderer, bitmap, area, key, "/cover.bmp"));
  file.close();
  // Change source after rasterization: the writer must reject the stale pixels.
  testStorage.files["/cover.bmp"][source.size() - 1] ^= 1;
  const auto changed = testStorage.files["/cover.bmp"];
  finish(cache);
  check(testStorage.files["/cover.bmp"] == changed);
  testStorage.files["/cover.bmp"] = source;
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  check(cache.prepare(renderer, bitmap, area, key, "/cover.bmp"));
  file.close();
  unsigned partial = 0;
  while (cache.writing() && testStorage.files["/cover.bmp"].size() == source.size()) {
    cache.step();
    check(++partial < 128);
  }
  cache.cancel();
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  check(!cache.load(file, renderer, area, key));
  file.close();
  check(!std::memcmp(testStorage.files["/cover.bmp"].data(), source.data(), source.size()));
  renderer.mode = GfxRenderer::GRAYSCALE_MSB;
  check(Cache::key(renderer, area) == 0);
  renderer.mode = GfxRenderer::BW;
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  renderer.setClipRect(0, 0, 5, 5);
  const auto oldClip = renderer.getClipRect();
  const auto clippedFrame = renderer.frame;
  check(cache.prepare(renderer, bitmap, area, key, "/cover.bmp"));
  check(renderer.getClipRect() == oldClip);
  check(renderer.frame == clippedFrame);
  file.close();
  finish(cache);
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  check(!cache.load(file, renderer, area, key));
  check(renderer.frame == clippedFrame);
  file.close();
  renderer.setClipRect(0, 0, 32767, 32767);
  testStorage.files["/cover.bmp"] = bmp(640, 1000, 2, false);
  check(Storage.openFileForRead("TEST", "/cover.bmp", file));
  check(!cache.prepare(renderer, bitmap, area, key, "/cover.bmp"));
  check(!cache.writing());
  file.close();
  // Reopening a validated native cover skips precisely the BMP prefix, preserving pixels.
  Cache::SourceStamp beforeOpen;
  const auto openMemo = [&] {
    beforeOpen = Cache::sourceStamp();
    return Storage.openFileForRead("TEST", "/memo.bmp", file);
  };
  testStorage.files["/memo.bmp"] = source;
  testContentRevision.bump();
  check(openMemo());
  check(cache.prepare(renderer, bitmap, area, key, "/memo.bmp"));
  file.close();
  finish(cache);
  const auto memoValid = testStorage.files["/memo.bmp"];
  check(openMemo());
  std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
  auto readStart = testStorage.readBytes;
  check(cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  const auto coldBytes = testStorage.readBytes - readStart;
  const auto exact = renderer.frame;
  file.close();
  check(openMemo());
  std::fill(renderer.frame.begin(), renderer.frame.end(), 0);
  readStart = testStorage.readBytes;
  check(cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  const auto warmBytes = testStorage.readBytes - readStart;
  check(coldBytes == warmBytes + source.size());
  check(renderer.frame == exact);
  std::printf("Validated native reopen: %llu -> %llu SD bytes; identical pixels\n", (unsigned long long)coldBytes,
              (unsigned long long)warmBytes);
  file.close();
  libraryui::CoverPrefetch sharedValidation;
  sharedValidation.begin(301, 302, 303);
  readStart = testStorage.readBytes;
  auto loaded = sharedValidation.load(0, 177, "/memo.bmp", key,
                                      renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height));
  check(loaded == libraryui::CoverPrefetch::Result::Ready);
  check(testStorage.readBytes - readStart == warmBytes);
  check(bool(sharedValidation.find(301, 302, 303, 0, 177, key)));
  ++testStorage.modified;  // FAT dates are deliberately not the trust boundary.
  testMediaRevision.bump();
  check(!sharedValidation.find(301, 302, 303, 0, 177, key));
  check(openMemo());
  readStart = testStorage.readBytes;
  check(cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  check(testStorage.readBytes - readStart == coldBytes);
  file.close();
  // A write with unchanged size/date invalidates source reuse.
  HalFile update;
  check(Storage.openFileForUpdate("TEST", "/memo.bmp", update));
  check(update.seek(source.size() - 1));
  uint8_t changedByte = testStorage.files["/memo.bmp"][source.size() - 1] ^ 1;
  check(update.write(&changedByte, 1) == 1);
  update.close();
  check(openMemo());
  check(!cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  check(renderer.frame == exact);
  file.close();
  testStorage.files["/memo.bmp"] = memoValid;
  testContentRevision.bump();
  check(openMemo());
  check(cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  file.close();
  // Pixel checksum remains mandatory even after source validation is reused.
  testStorage.files["/memo.bmp"][source.size() + 1] ^= 1;
  check(openMemo());
  check(!cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  check(renderer.frame == exact);
  file.close();
  testStorage.files["/memo.bmp"] = memoValid;
  testContentRevision.bump();
  check(openMemo());
  check(cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  file.close();
  testStorage.onRead = [] { testContentRevision.bump(); };
  check(openMemo());
  check(!cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  check(renderer.frame == exact);
  file.close();
  libraryui::CoverPrefetch interrupted;
  interrupted.begin(401, 402, 403);
  testStorage.onRead = [] { testMediaRevision.bump(); };
  auto result = libraryui::CoverPrefetch::Result::Pending;
  unsigned attempts = 0;
  while (result == libraryui::CoverPrefetch::Result::Pending && ++attempts < 128)
    result = interrupted.load(0, 177, "/memo.bmp", key,
                              renderer.getPackedRegionByteSize(area.x, area.y, area.width, area.height));
  check(result == libraryui::CoverPrefetch::Result::Unavailable);
  check(!interrupted.find(401, 402, 403, 0, 177, key));
  // A replacement after open cannot bind an old handle to the new path revision.
  testStorage.files["/memo.bmp"] = memoValid;
  testContentRevision.bump();
  check(openMemo());
  testContentRevision.bump();
  check(!cache.load(file, renderer, area, key, true, "/memo.bmp", &beforeOpen));
  file.close();
  StorageRevision saturated(UINT32_MAX - 1);
  saturated.bump();
  check(saturated.get() == UINT32_MAX);
  saturated.bump();
  check(saturated.get() == 0);
  saturated.bump();
  check(saturated.get() == 0);
  std::printf("Native covers: %u orientation/palette/polarity/row-order cases; %u checks, %u failures\n", cases, checks,
              failures);
  return failures ? 1 : 0;
}
