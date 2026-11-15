#include <Epub/PageReadBudget.h>
#include <Epub/blocks/ImageBlock.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <Serialization.h>

#include <array>
#include <cstdio>
#include <filesystem>

ImageToFramebufferDecoder* ImageDecoderFactory::getDecoder(const std::string&) { return nullptr; }

int main() {
  unsigned checks = 0, failures = 0;
  const auto check = [&](bool result) {
    ++checks;
    if (!result) {
      ++failures;
      std::printf("Failed check %u\n", checks);
    }
  };
  const auto folder = std::filesystem::temp_directory_path() / "crossnext-reader-speed-images";
  std::filesystem::create_directories(folder);
  const auto writeCache = [&](int number, uint8_t value) {
    const auto path = (folder / ("image" + std::to_string(number) + ".jpg.8x8.pxc")).string();
    HalFile file;
    check(file.open(path.c_str(), "wb"));
    const uint16_t size = 8;
    file.write(&size, 2);
    file.write(&size, 2);
    std::array<uint8_t, 16> pixels;
    pixels.fill(value);
    file.write(pixels.data(), pixels.size());
  };
  for (int n = 0; n < 3; ++n) writeCache(n, uint8_t(0x1b + n * 0x20));
  const auto render = [&](GfxRenderer& renderer) {
    for (int n = 0; n < 3; ++n) {
      ImageBlock image((folder / ("image" + std::to_string(n) + ".jpg")).string(), "", 8, 8);
      image.render(renderer, n * 8, 0);
    }
  };
  for (int rotation = 0; rotation < 4; ++rotation) {
    GfxRenderer renderer;
    renderer.orientation = GfxRenderer::Orientation(rotation);
    std::array<std::array<uint8_t, 320>, 3> expected;
    HalMemory::fail = true;
    for (int mode = 0; mode < 3; ++mode) {
      renderer.mode = GfxRenderer::RenderMode(mode);
      renderer.frame.fill(0xff);
      render(renderer);
      expected[mode] = renderer.frame;
      ImageBlock::releaseRenderCache();
    }
    HalMemory::fail = false;
    const int opens = Storage.readOpens;
    for (int mode = 0; mode < 3; ++mode) {
      renderer.mode = GfxRenderer::RenderMode(mode);
      renderer.frame.fill(0xff);
      render(renderer);
      check(renderer.frame == expected[mode]);
    }
    check(Storage.readOpens - opens == 3);
    check(HalMemory::active == 3);
    ImageBlock::endRenderCache();
    ImageBlock::beginRenderCache();
    renderer.frame.fill(0xff);
    render(renderer);
    check(renderer.frame == expected[2]);
    check(Storage.readOpens - opens == 3);
    ImageBlock::endRenderCache();
    ++Storage.content;
    ImageBlock::beginRenderCache();
    renderer.frame.fill(0xff);
    render(renderer);
    check(renderer.frame == expected[2]);
    check(Storage.readOpens - opens == 6);
    ImageBlock::endRenderCache();
    ImageBlock::releaseRenderCache();
    check(HalMemory::active == 0);
  }
  // Speculation only fills the existing PSRAM slots; it must never paint.
  for (int rotation = 0; rotation < 4; ++rotation) {
    GfxRenderer renderer;
    renderer.orientation = GfxRenderer::Orientation(rotation);
    ImageBlock image((folder / "image0.jpg").string(), "", 8, 8);
    std::array<std::array<uint8_t, 320>, 3> expected;
    for (int mode = 0; mode < 3; ++mode) {
      HalMemory::fail = true;
      renderer.mode = GfxRenderer::RenderMode(mode);
      renderer.frame.fill(0xff);
      image.render(renderer, 0, 0);
      expected[mode] = renderer.frame;
      ImageBlock::releaseRenderCache();
    }
    HalMemory::fail = false;
    renderer.frame.fill(0xa5);
    const auto untouched = renderer.frame;
    ImageBlock::beginRenderCache();
    const auto opens = Storage.readOpens;
    check(image.prefetchPixels(renderer, [](void*) { return true; }, nullptr));
    check(renderer.frame == untouched);
    check(Storage.readOpens == opens + 1);
    ++Storage.state;
    for (int mode = 0; mode < 3; ++mode) {
      ImageBlock::beginRenderCache();
      renderer.mode = GfxRenderer::RenderMode(mode);
      renderer.frame.fill(0xff);
      image.render(renderer, 0, 0);
      check(renderer.frame == expected[mode]);
      check(Storage.readOpens == opens + 1);
      ImageBlock::endRenderCache();
    }
    ImageBlock::releaseRenderCache();
    HalFileTest::failNextPayloadRead = true;
    renderer.mode = GfxRenderer::RenderMode::BW;
    renderer.frame.fill(0xff);
    ImageBlock::beginRenderCache();
    image.render(renderer, 0, 0);
    check(renderer.frame == expected[0]);
    check(!HalFileTest::failNextPayloadRead && HalMemory::active == 0);
    ImageBlock::endRenderCache();
    HalFileTest::onRead = []() { ++Storage.content; };
    renderer.frame = untouched;
    check(!image.prefetchPixels(renderer, [](void*) { return true; }, nullptr));
    check(renderer.frame == untouched && HalMemory::active == 0);
  }
  {
    const auto largePath = (folder / "large.jpg.128x256.pxc").string();
    HalFile file;
    check(file.open(largePath.c_str(), "wb"));
    serialization::writePod(file, uint16_t(128));
    serialization::writePod(file, uint16_t(256));
    const std::vector<uint8_t> pixels(8192, 0x1b);
    check(file.write(pixels.data(), pixels.size()) == pixels.size());
    file.close();
    ImageBlock image((folder / "large.jpg").string(), "", 128, 256);
    GfxRenderer renderer;
    const auto untouched = renderer.frame;
    int work = 2;
    check(!image.prefetchPixels(renderer, [](void* ctx) { return --*static_cast<int*>(ctx) >= 0; }, &work));
    check(renderer.frame == untouched && HalMemory::active == 0);
    check(image.prefetchPixels(renderer, [](void*) { return true; }, nullptr));
    check(renderer.frame == untouched && HalMemory::active == 1);
    ImageBlock::releaseRenderCache();
  }
  HalMemory::psram = false;
  GfxRenderer fallback;
  render(fallback);
  ImageBlock::releaseRenderCache();
  check(HalMemory::active == 0);
  HalMemory::psram = true;

  const auto path = (folder / "block.bin").string();
  const auto writeBlock = [&](uint32_t length, int16_t width) {
    HalFile out;
    check(out.open(path.c_str(), "wb"));
    if (length == UINT32_MAX) {
      serialization::writePod(out, length);
      return;
    }
    serialization::writeString(out, std::string(length, 'a'));
    serialization::writeString(out, std::string("source.jpg"));
    serialization::writePod(out, width);
    serialization::writePod(out, int16_t(8));
  };
  const auto readBlock = [&](size_t bytes) {
    HalFile in;
    check(in.open(path.c_str(), "rb"));
    PageReadBudget budget(bytes);
    return ImageBlock::deserialize(in, &budget);
  };
  writeBlock(8, 8);
  check(bool(readBlock(24 * 1024)));
  check(!readBlock(16));
  writeBlock(UINT32_MAX, 8);
  check(!readBlock(24 * 1024));
  writeBlock(1025, 8);
  check(!readBlock(24 * 1024));
  writeBlock(8, 0);
  check(!readBlock(24 * 1024));
  ImageBlock oversized((folder / "oversized.jpg").string(), "source.jpg", 32767, 32767);
  unsigned extracts = 0;
  const int opensBeforeOversized = Storage.readOpens;
  const auto extract = [](void* count, const char*, const char*) {
    ++*static_cast<unsigned*>(count);
    return false;
  };
  check(!oversized.prepareCache(fallback, &extracts, extract, 0, 0, [](void*) { return true; }));
  check(extracts == 0);
  check(Storage.readOpens == opensBeforeOversized);
  ImageBlock::releaseRenderCache();
  std::printf("Reader image caches: %u checks, %u failures; 3 cached-image opens across BW and both gray planes\n",
              checks, failures);
  return failures ? 1 : 0;
}
