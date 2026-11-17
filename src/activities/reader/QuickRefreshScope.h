#pragma once

#include <GfxRenderer.h>

#include <cstdint>

// Asks the panel for its short fast waveform while frames follow a finger (a
// selection being dragged), and restores the ordinary waveform on exit. Short
// frames are a draft: the panel driver brings the pixels they touched to full
// density on the next ordinary refresh, so the frame drawn when the finger
// lifts must be presented outside such a scope. Zero frames leaves the panel
// alone; panels without a short waveform ignore the request.
class QuickRefreshScope {
 public:
  QuickRefreshScope(const GfxRenderer& renderer, uint8_t frames) : renderer(renderer), frames(frames) {
    if (frames) renderer.setQuickRefresh(frames);
  }
  ~QuickRefreshScope() {
    if (frames) renderer.setQuickRefresh(0);
  }
  QuickRefreshScope(const QuickRefreshScope&) = delete;
  QuickRefreshScope& operator=(const QuickRefreshScope&) = delete;

 private:
  const GfxRenderer& renderer;
  const uint8_t frames;
};
