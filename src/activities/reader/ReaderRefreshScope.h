#pragma once

#include <GfxRenderer.h>

// Tells the panel that a book text page is being presented, for the duration of
// one page render. On the X4 Pro (UC8279) the base and the anti-aliasing masks
// then go out in a single waveform instead of a base refresh followed by a gray
// pass. Inactive scopes, and panels without such a waveform, change nothing.
class ReaderRefreshScope {
 public:
  ReaderRefreshScope(const GfxRenderer& renderer, bool textPage) : renderer(renderer), active(textPage) {
    if (active) renderer.setReaderRefresh(true);
  }
  ~ReaderRefreshScope() {
    if (active) renderer.setReaderRefresh(false);
  }
  ReaderRefreshScope(const ReaderRefreshScope&) = delete;
  ReaderRefreshScope& operator=(const ReaderRefreshScope&) = delete;

 private:
  const GfxRenderer& renderer;
  const bool active;
};
