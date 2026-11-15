#include <Arduino.h>
#include <Performance.h>

#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

std::string logText;
void logPrintf(const char*, const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof(line), format, args);
  va_end(args);
  logText += line;
  logText += '\n';
}

bool locked = false;
void (*beforeLock)() = nullptr;
struct RenderLock {
  RenderLock() {
    assert(!locked);
    if (beforeLock) beforeLock();
    locked = true;
  }
  ~RenderLock() { locked = false; }
};
struct SectionStub {
  int currentPage = 0, pageCount = 3;
  bool building = false, finished = false;
  bool isBuilding() const { return building; }
  bool layoutFinished() const { return finished; }
};
struct BookStub {
  int getSpineItemsCount() const { return 3; }
};
struct RendererStub {
  void waitRefreshComplete() { testTimeMs += 20; }
  void cleanupGrayscaleWithFrameBuffer() { testTimeMs += 20; }
};
struct EpubReaderActivity {
  std::unique_ptr<SectionStub> section = std::make_unique<SectionStub>();
  std::unique_ptr<BookStub> epub = std::make_unique<BookStub>();
  RendererStub renderer;
  int currentSpineIndex = 0, nextPageNumber = 0;
  unsigned long lastPageTurnTime = 0;
  std::optional<uint16_t> pendingPageJump;
  bool finalPagePresented = true, textFormatPending = false, overlayRefreshPending = false, formatReflowPending = false;
  uint32_t renderedOperation = 0;
  bool overlayPaintPending = false;
  void normalOverlayTick() {
#include "overlay-tick-under-test.inc"
  }
  void clearDeferredReposition() { assert(locked); }
  void resetSection() {
    assert(locked);
    section.reset();
  }
  bool pageTurn(bool);
  void settleOverlayRefresh();
#include "presentation-under-test.inc"
};
#include "settle-under-test.inc"
#include "turn-under-test.inc"

struct SectionTargetFixture {
  struct Entry {
    uint32_t visibleTextOffset;
  };
  struct Build {
    std::vector<Entry> lut;
  };
  std::unique_ptr<Build> build_ = std::make_unique<Build>();
#include "offset-under-test.inc"
};

int main() {
  EpubReaderActivity reader;
  assert(!reader.pageTurn(false));
  assert(performance::current() == 0);  // rejected remote Previous must leave no stale measurement
  beforeLock = [] {
    // Deterministic old-repaint interleaving before the input acquires RenderLock.
    assert(performance::current() == 0);
    testTimeMs += 7;
    beforeLock = nullptr;
  };
  assert(reader.pageTurn(true));
  assert(reader.section->currentPage == 1);
  const auto operation = performance::current();
  assert(operation && !reader.presentationComplete());
  reader.renderedOperation = operation;
  reader.finalPagePresented = false;
  assert(!reader.presentationComplete());  // intermediate failed-load render
  reader.finalPagePresented = true;
  reader.textFormatPending = true;
  assert(!reader.presentationComplete());
  reader.textFormatPending = false;
  reader.formatReflowPending = true;
  assert(!reader.presentationComplete());
  reader.formatReflowPending = false;
  reader.overlayRefreshPending = true;
  assert(!reader.presentationComplete());
  reader.overlayPaintPending = true;
  reader.normalOverlayTick();
  assert(performance::current() == 0);
  assert(logText.find("dur=27000") != std::string::npos);  // input wait plus async final waveform
  assert(reader.presentationComplete());
  reader.renderedOperation = performance::begin(performance::Operation::Format, &reader);
  reader.overlayRefreshPending = true;
  {
    RenderLock lock;
    reader.settleOverlayRefresh();
  }
  assert(performance::current() == 0);
  reader.formatReflowPending = true;
  assert(!reader.pageTurn(true));
  reader.formatReflowPending = false;
  reader.section->building = reader.section->finished = true;
  reader.section->currentPage = reader.section->pageCount - 1;
  assert(reader.pageTurn(true) && reader.currentSpineIndex == 1 && !reader.section);
  for (const auto& offsets : {std::vector<uint32_t>{0, 5, 5, 5, 10, 12}, std::vector<uint32_t>{0, 5, 5},
                              std::vector<uint32_t>{0, 0, 0, 20}}) {
    const uint32_t target = offsets[1];
    int selected[2]{};
    for (int mode = 0; mode < 2; ++mode) {
      SectionTargetFixture state;
      size_t at = 0;
      const size_t increment = mode ? 8 : 1;
      while (at < offsets.size() && !state.buildReachedVisibleTextOffset(target)) {
        const size_t end = std::min(offsets.size(), at + increment);
        while (at < end) state.build_->lut.push_back({offsets[at++]});
      }
      for (size_t i = 0; i < state.build_->lut.size(); ++i)
        if (state.build_->lut[i].visibleTextOffset <= target) selected[mode] = int(i);
    }
    assert(selected[0] == selected[1]);
    int expected = 0;
    for (size_t i = 0; i < offsets.size(); ++i)
      if (offsets[i] <= target) expected = int(i);
    assert(selected[0] == expected);
  }
  puts("Reader interleaving, rejected turns, retry/async completion and equal-offset reflow regression passed");
}
