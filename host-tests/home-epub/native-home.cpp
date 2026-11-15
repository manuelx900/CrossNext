#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>

#include <cassert>
#include <cstdio>
#include <vector>

#include "apps_local/home/CoverBitmapPainter.h"
#include "components/HomeCoverCache.h"
using Rect = freeink::ui::Rect;
enum class Color { LightGray };
struct Gui {
  bool drawCoverThumbFill(GfxRenderer& r, Bitmap& b, Rect a, int) { return homecover::paintBitmap(r, b, a, true); }
  void drawCoverPlaceholder(GfxRenderer& r, Rect a) { r.fillRect(a.x, a.y, a.width, a.height, false); }
} GUI;
namespace homecover {
void recordSource(const char*, HalFile&, bool) {}
}  // namespace homecover
#include "home-cover-production.inc"
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
int main() {
  unsigned cases = 0;
  for (auto orientation : {GfxRenderer::Portrait, GfxRenderer::LandscapeClockwise, GfxRenderer::PortraitInverted,
                           GfxRenderer::LandscapeCounterClockwise})
    for (bool inverted : {false, true})
      for (unsigned depth : {1u, 2u})
        for (bool top : {false, true}) {
          GfxRenderer r;
          r.orientation = orientation;
          r.inverted = inverted;
          const auto area = freeink::ui::makeRect(14, 21, 100, 160);
          testStorage.files["/cover.bmp"] = bmp(100, 160, depth, top);
          testContentRevision.bump();
          HomeCoverCache first(r);
          first.begin(true);
          first.prepare();
          assert(first.paint(area, 0, "/cover.bmp", true, true) && first.hasImage(0));
          const auto expected = r.frame;
          const auto reads = testStorage.readBytes;
          assert(first.paint(area, 0, "/cover.bmp", true, true));
          assert(testStorage.readBytes == reads);
          libraryui::PreparedCoverCache writer;
          HalFile file;
          assert(Storage.openFileForRead("T", "/cover.bmp", file));
          Bitmap bitmap(file);
          assert(writer.prepare(r, bitmap, area, libraryui::PreparedCoverCache::key(r, area), "/cover.bmp"));
          while (writer.writing()) writer.step();
          HomeCoverCache prepared(r);
          prepared.begin(true);
          prepared.prepare();
          std::fill(r.frame.begin(), r.frame.end(), 0);
          assert(prepared.paint(area, 0, "/cover.bmp", true, true) && prepared.hasImage(0));
          assert(r.frame == expected);
          testStorage.files["/cover.bmp"].back() ^= 1;
          testContentRevision.bump();
          prepared.invalidate();
          std::fill(r.frame.begin(), r.frame.end(), 0);
          assert(prepared.paint(area, 0, "/cover.bmp", true, true));
          assert(r.frame == expected);
          ++cases;
        }
  std::printf(
      "Production Home native painter: %u orientation/polarity/palette/row-order cases identical; region hit zero "
      "reads; damaged footer falls back\n",
      cases);
}
