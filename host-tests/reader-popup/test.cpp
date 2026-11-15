#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define LOG_DBG(...) ((void)0)
struct RenderLock {};
struct PanelSpan {};
struct GfxRenderer {
  static constexpr size_t BW_BUFFER_CHUNK_SIZE = 16;
  int frameBufferSize = 48;
  uint8_t pixels[48]{};
  uint8_t* frameBuffer = pixels;
  std::array<uint8_t*, 3> bwBufferChunks{};
  int releases = 0;
  struct {
    int resyncs = 0;
    void cleanupGrayscaleBuffers(uint8_t*) { ++resyncs; }
  } display;
  bool copyStoredBwBuffer();
  void restoreBwBuffer(bool resyncPanelBaseline);
  void freeBwBufferChunks() {
    for (auto& p : bwBufferChunks) {
      if (p) {
        std::free(p);
        ++releases;
      }
      p = nullptr;
    }
  }
};
struct EpubReaderActivity {
  GfxRenderer& renderer;
  explicit EpubReaderActivity(GfxRenderer& value) : renderer(value) {}
  bool overlayPageStored = false;
  int updates = 0, sheets = 0, refreshes = 0, settled = 0;
  struct Popup {
    int paints = 0;
    void render(GfxRenderer&) { ++paints; }
  } overlayPopup;
  void settleOverlayRefresh() { ++settled; }
  void renderOverlay() { ++sheets; }
  void requestUpdate() { ++updates; }
  void pushOverlayRefresh() { ++refreshes; }
  void paintOverlayPopup();
};
#include "production.inc"
int main() {
  GfxRenderer gfx;
  std::memset(gfx.pixels, 0xcc, sizeof(gfx.pixels));
  assert(!gfx.copyStoredBwBuffer());
  assert(gfx.pixels[0] == 0xcc);
  EpubReaderActivity reader{gfx};
  reader.paintOverlayPopup();
  assert(reader.updates == 1 && !reader.overlayPopup.paints && !reader.refreshes);
  reader.overlayPageStored = true;
  reader.paintOverlayPopup();
  assert(reader.updates == 2 && !reader.overlayPageStored && !reader.overlayPopup.paints);
  for (auto& p : gfx.bwBufferChunks) {
    p = static_cast<uint8_t*>(std::malloc(16));
    assert(p);
    std::memset(p, 0x55, 16);
  }
  const auto original = gfx.bwBufferChunks;
  reader.overlayPageStored = true;
  for (int n = 0; n < 20; ++n) {
    std::memset(gfx.pixels, 0xff, sizeof(gfx.pixels));
    reader.paintOverlayPopup();
    assert(gfx.bwBufferChunks == original && !gfx.releases && !gfx.display.resyncs);
    assert(gfx.pixels[0] == 0x55 && gfx.pixels[47] == 0x55);
  }
  assert(reader.sheets == 20 && reader.overlayPopup.paints == 20 && reader.refreshes == 20);
  gfx.restoreBwBuffer(false);
  assert(gfx.releases == 3 && !gfx.display.resyncs && !gfx.copyStoredBwBuffer());
  for (auto& p : gfx.bwBufferChunks) {
    p = static_cast<uint8_t*>(std::malloc(16));
    assert(p);
    std::memset(p, 0x33, 16);
  }
  gfx.restoreBwBuffer(true);
  assert(gfx.releases == 6 && gfx.display.resyncs == 1 && gfx.pixels[0] == 0x33);
  std::puts(
      "Reader popup: retained snapshot, repeated paints, allocation failure fallback and baseline ownership passed");
}
