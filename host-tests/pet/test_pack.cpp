#include <FreeInkUIDisplayTarget.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/apps_local/ui/SdBitmapPack.h"

static void referenceDraw(freeink::ui::DrawTarget& target, freeink::ui::Rect area, const char* path, sdart::Image image,
                          freeink::ui::BitmapMode mode, freeink::ui::Color color) {
  namespace fui = freeink::ui;
  auto* file = std::fopen((std::string("resources") + path).c_str(), "rb");
  assert(file && !std::fseek(file, long(image.offset), SEEK_SET));
  std::vector<uint8_t> source(image.bytes);
  assert(std::fread(source.data(), 1, source.size(), file) == source.size());
  std::fclose(file);
  auto rect = area;
  if (mode == fui::BitmapMode::Contain) {
    const int width = std::min(int(area.width), int(area.height) * image.width / image.height);
    const int height = width * image.height / image.width;
    rect = fui::makeRect(area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height);
  }
  for (int y = 0; y < image.height; ++y) {
    const int top = rect.y + y * rect.height / image.height;
    const int bottom = rect.y + (y + 1) * rect.height / image.height;
    if (bottom > top)
      target.bitmap(fui::makeRect(rect.x, top, rect.width, bottom - top),
                    {source.data() + y * ((image.width + 7) / 8), image.width, 1, fui::BitmapFormat::BW1},
                    fui::BitmapMode::Stretch, fui::Paint::solid(color));
  }
}

int main(int argc, char** argv) {
  namespace fui = freeink::ui;
  static constexpr struct {
    const char* path;
    unsigned count;
  } packs[] = {{"/apps/pet/graphics.bin", 64},   {"/apps/pet/home.bin", 45},    {"/apps/pet/habitat.bin", 560},
               {"/apps/pet/breathing.bin", 474}, {"/apps/pet/farewell.bin", 6}, {"/apps/pet/collection.bin", 1}};
  std::vector<uint8_t> pixels(128 * 128 / 8, 255);
  fui::DisplayTarget target(pixels.data(), 128, 128, 16);
  for (unsigned mode = 0; mode < 3; ++mode) {
    sdartMemory::available = mode != 1;
    sdartMemory::denyAllocation = mode == 2;
    sdart::Pack pack;
    for (const auto& asset : packs) {
      assert(pack.open(asset.path, asset.count));
      assert(sdartMemory::liveBytes == (mode == 0 ? asset.count * 16 + 4096 : 0));
      for (unsigned id : {0u, asset.count / 2, asset.count - 1}) {
        const auto image = pack.image(id);
        assert(image.width > 1 && image.width <= sdart::MAX_WIDTH && image.height <= sdart::MAX_HEIGHT);
        pack.draw(target, fui::makeRect(0, 0, 128, 128), id);
        assert(pack.ready());
      }
      pack.close();
      assert(!pack.ready() && !sdartMemory::liveBytes);
    }
    assert(!pack.open("/apps/pet/collection.bin", 2) && !sdartMemory::liveBytes);
    assert(pack.open("/apps/pet/collection.bin", 1));
    pack.image(1);
    assert(!pack.ready() && !sdartMemory::liveBytes);
    if (argc > 1) {
      assert(!pack.open(argv[1], 45));
      assert(pack.open(argv[1], 39));
      for (unsigned id : {0u, 38u}) {
        assert(pack.image(id).width > 1);
        pack.draw(target, fui::makeRect(0, 0, 128, 128), id);
        assert(pack.ready());
      }
      pack.close();
      assert(!sdartMemory::liveBytes);
    }
  }
  sdartMemory::available = true;
  sdartMemory::denyAllocation = false;
  for (const auto& asset : packs) {
    sdart::Pack pack;
    const unsigned ids[] = {0, asset.count / 2};
    std::vector<uint8_t> expected[2];
    assert(pack.open(asset.path, asset.count));
    for (unsigned orientation = 0; orientation < 4; ++orientation)
      for (auto mode : {fui::BitmapMode::Contain, fui::BitmapMode::Stretch})
        for (auto color : {fui::Color::Black, fui::Color::White, fui::Color::LightGray})
          for (auto area : {fui::makeRect(0, 0, 128, 128), fui::makeRect(11, 9, 93, 57), fui::makeRect(0, 0, 7, 5),
                            fui::makeRect(-13, 3, 150, 117)}) {
            std::vector<uint8_t> reference(pixels.size(), color == fui::Color::White ? 0 : 255);
            pixels = reference;
            fui::DisplayTarget actual(pixels.data(), 128, 128, 16, fui::Orientation(orientation));
            fui::DisplayTarget expected(reference.data(), 128, 128, 16, fui::Orientation(orientation));
            actual.setClipRect(fui::makeRect(5, 7, 100, 109));
            expected.setClipRect(fui::makeRect(5, 7, 100, 109));
            const auto image = pack.image(ids[1]);
            referenceDraw(expected, area, asset.path, image, mode, color);
            pack.draw(actual, area, ids[1], mode, color);
            assert(pack.ready() && pixels == reference);
          }
    for (unsigned i = 0; i < 2; ++i) {
      target.fill(fui::makeRect(0, 0, 128, 128), fui::Paint::solid(fui::Color::White));
      pack.draw(target, fui::makeRect(0, 0, 128, 128), ids[i]);
      expected[i] = pixels;
    }
    const auto before = sdartReadCalls;
    for (auto id : ids) pack.draw(target, fui::makeRect(0, 0, 128, 128), id);
    const auto directReads = sdartReadCalls - before;
    pack.close();
    assert(pack.open(asset.path, asset.count, 64 * 1024));
    assert(sdartMemory::liveBytes == asset.count * 16 + 4096 + 64 * 1024);
    for (unsigned i = 0; i < 2; ++i) {
      target.fill(fui::makeRect(0, 0, 128, 128), fui::Paint::solid(fui::Color::White));
      pack.draw(target, fui::makeRect(0, 0, 128, 128), ids[i]);
      assert(pack.ready() && pixels == expected[i]);
    }
    const auto warmReads = sdartReadCalls, warmSeeks = sdartSeekCalls;
    for (unsigned i = 0; i < 2; ++i) {
      target.fill(fui::makeRect(0, 0, 128, 128), fui::Paint::solid(fui::Color::White));
      pack.draw(target, fui::makeRect(0, 0, 128, 128), ids[i]);
      assert(pack.ready() && pixels == expected[i]);
    }
    assert(sdartReadCalls == warmReads && sdartSeekCalls == warmSeeks);
    std::printf("PET sprite cache %s: warm reads %zu -> 0; identical pixels\n", asset.path, directReads);
    if (asset.count > 16) {
      for (unsigned id = 0; id < 12; ++id) pack.draw(target, fui::makeRect(0, 0, 64, 64), id);
      const auto pageReads = sdartReadCalls;
      for (unsigned id = 0; id < 12; ++id) pack.draw(target, fui::makeRect(0, 0, 64, 64), id);
      if (asset.count == 560) {
        assert(sdartReadCalls == pageReads);
        std::puts("PET home catalogue: twelve cached thumbnails, warm reads 0");
      }
      for (unsigned id = 0; id < 18; ++id) pack.draw(target, fui::makeRect(0, 0, 128, 128), id);
      target.fill(fui::makeRect(0, 0, 128, 128), fui::Paint::solid(fui::Color::White));
      pack.draw(target, fui::makeRect(0, 0, 128, 128), 0);
      assert(pack.ready() && pixels == expected[0]);
      sdartReadFailure = true;
      pack.draw(target, fui::makeRect(0, 0, 128, 128), asset.count - 1);
      assert(!pack.ready() && !sdartMemory::liveBytes);
      sdartReadFailure = false;
    }
    pack.close();
    assert(!sdartMemory::liveBytes);
    sdartMemory::allocationLimit = asset.count * 16 + 4096;
    assert(pack.open(asset.path, asset.count, 64 * 1024));
    assert(sdartMemory::liveBytes == sdartMemory::allocationLimit);
    pack.draw(target, fui::makeRect(0, 0, 128, 128), 0);
    assert(pack.ready());
    pack.close();
    sdartMemory::allocationLimit = size_t(-1);
  }
  std::puts("PET real SD loader: six packs, actual dimensions/CRC, PSRAM/small-read/OOM fallback and close PASS");
}
