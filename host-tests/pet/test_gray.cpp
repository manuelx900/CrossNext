#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>

#include "../../src/apps_local/pet/PetGrayTarget.h"

namespace fui = freeink::ui;
int main() {
  std::array<std::array<uint8_t, 8>, 2> pixels{};
  static constexpr fui::Color colors[] = {fui::Color::Black, fui::Color::DarkGray, fui::Color::LightGray,
                                          fui::Color::White};
  for (unsigned plane = 0; plane < 2; ++plane) {
    pixels[plane].fill(255);
    fui::DisplayTarget target(pixels[plane].data(), 8, 8, 1);
    petui::GrayTarget gray(target, plane == 1);
    for (int n = 0; n < 4; ++n) gray.fill(fui::makeRect(n * 2, 0, 2, 8), fui::Paint::dither(colors[n]));
    // A panel erases room artwork, irrespective of the previous plane's bits.
    gray.fill(fui::makeRect(0, 4, 8, 4), fui::Paint::solid(fui::Color::White));
    gray.fill(fui::makeRect(0, 0, 8, 8), fui::Paint::solid(fui::Color::Transparent));
    gray.fill(fui::makeRect(0, 0, 8, 8), fui::Paint::none());
    assert(gray.setClipRect(fui::makeRect(2, 5, 4, 2)));
    gray.fill(fui::makeRect(0, 4, 8, 4), fui::Paint::solid(fui::Color::DarkGray));
    assert(gray.clipRect().x == 2 && gray.clipRect().width == 4);
  }
  for (unsigned y = 0; y < 8; ++y)
    for (unsigned x = 0; x < 8; ++x) {
      const unsigned bit = 128 >> x;
      const bool lsb = pixels[0][y] & bit, msb = pixels[1][y] & bit;
      const unsigned level = lsb ? (msb ? 3 : 1) : (msb ? 2 : 0);
      const unsigned expected = y < 4 ? x / 2 : (y >= 5 && y < 7 && x >= 2 && x < 6 ? 1 : 3);
      assert(level == expected);
    }
  std::puts("PET absolute gray: four solid levels, panel erasure, transparency and clipping passed");
}
