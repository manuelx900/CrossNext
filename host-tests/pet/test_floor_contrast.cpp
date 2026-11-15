#include <FreeInkUIDisplayTarget.h>

#include <cassert>
#include <cstdio>
#include <vector>

#include "../../src/apps_local/pet/PetRoomBackground.h"
#include "../../src/apps_local/ui/SdBitmapPack.h"

namespace fui = freeink::ui;
unsigned ink(const std::vector<uint8_t>& pixels, int width, int first, int last) {
  unsigned total = 0;
  for (int y = first; y < last; ++y)
    for (int x = 0; x < width; ++x) total += !(pixels[y * ((width + 7) / 8) + x / 8] & (0x80u >> (x % 8)));
  return total;
}
void write(const char* folder, const char* label, const std::vector<uint8_t>& pixels, int width, int height) {
  char path[256];
  std::snprintf(path, sizeof(path), "%s/%s-%d.pbm", folder, label, width);
  FILE* file = std::fopen(path, "wb");
  assert(file);
  std::fprintf(file, "P4\n%d %d\n", width, height);
  for (auto byte : pixels) std::fputc(byte ^ 255, file);
  std::fclose(file);
}
int main(int argc, char** argv) {
  assert(argc == 2);
  const auto gray = fui::Paint::dither(fui::Color::LightGray);
  for (int phase = 0; phase < 4; ++phase) {
    std::vector<uint8_t> pixels(64 * 64 / 8, 255), reference(pixels);
    fui::DisplayTarget target(pixels.data(), 64, 64, 8, fui::Orientation::LandscapeCounterClockwise);
    fui::DisplayTarget original(reference.data(), 64, 64, 8, fui::Orientation::LandscapeCounterClockwise);
    petui::roombackground::FloorInkTarget floor(target, 32);
    original.fill(fui::makeRect(0, phase, 64, 1), gray);
    floor.fill(fui::makeRect(0, phase, 64, 1), gray);
    assert(pixels == reference);
    floor.fill(fui::makeRect(0, 32 + phase, 64, 1), gray);
    assert(ink(pixels, 64, 32 + phase, 33 + phase) > 0);
    floor.fill(fui::makeRect(phase, 40, 1, 24), gray);
    assert(ink(pixels, 64, 40, 64) > 0);
    floor.fill(fui::makeRect(0, 31, 64, 3), gray);
    assert(ink(pixels, 64, 32, 33) > 0 && ink(pixels, 64, 33, 34) > 0);
    const auto clip = fui::makeRect(4, 4, 12, 12);
    assert(floor.setClipRect(clip));
    assert(target.clipRect().x == clip.x && target.clipRect().width == clip.width);
  }
  sdart::Pack pack;
  assert(pack.open("resources/apps/pet/home.bin", petui::HOME_BACKGROUNDS_COUNT));
  for (bool wide : {false, true}) {
    const int width = wide ? 800 : 480, height = wide ? 480 : 800;
    const int stride = (width + 7) / 8, floorTop = height * (wide ? 80 : 56) / 100;
    for (int phase = 0; phase < 4; ++phase) {
      std::vector<uint8_t> before(stride * (height + 4), 255), after(before);
      fui::DisplayTarget raw(before.data(), width, height + 4, stride, fui::Orientation::LandscapeCounterClockwise);
      fui::DisplayTarget corrected(after.data(), width, height + 4, stride,
                                   fui::Orientation::LandscapeCounterClockwise);
      const auto bounds = fui::makeRect(0, phase, width, height);
      const int columns = wide ? 3 : 2, rows = wide ? 1 : 2;
      const unsigned first = wide ? petui::ROOM_LANDSCAPE_ART : petui::ROOM_PORTRAIT_ART;
      for (int y = 0; y < rows; ++y)
        for (int x = 0; x < columns; ++x) {
          const int left = x * width / columns, top = phase + y * height / rows;
          pack.draw(raw,
                    fui::makeRect(left, top, (x + 1) * width / columns - left, phase + (y + 1) * height / rows - top),
                    first + y * columns + x, fui::BitmapMode::Stretch);
        }
      petui::Model model;
      model.fullRoomBackgrounds = true;
      model.scenes = {&pack, [](void* context, fui::DrawTarget& target, fui::Rect box, uint32_t id, bool) {
                        auto& source = *static_cast<sdart::Pack*>(context);
                        source.draw(target, box, id, fui::BitmapMode::Stretch);
                        return source.ready();
                      }};
      assert(petui::roombackground::tiles(corrected, model, bounds, wide));
      for (int y = 0; y < floorTop + phase; ++y)
        for (int x = 0; x < stride; ++x) assert(before[y * stride + x] == after[y * stride + x]);
      unsigned recovered = 0;
      for (int y = floorTop + phase; y < height + phase; ++y)
        recovered += ink(before, width, y, y + 1) == 0 && ink(after, width, y, y + 1) > unsigned(width / 4);
      assert(recovered > 0);
      std::printf("Floor %dx%d phase%d: %u previously blank plank rows restored\n", width, height, phase, recovered);
      if (!phase) {
        write(argv[1], "floor-before", before, width, height);
        write(argv[1], "floor-after", after, width, height);
      }
    }
  }
  std::puts("Floor contrast: pixel phases, wall preservation, clipping and real room pack passed");
}
