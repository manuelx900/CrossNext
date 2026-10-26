#include "ReaderSession.h"

#include <algorithm>

namespace {
constexpr uint32_t MAX_DWELL_MS = 30 * 60 * 1000;
}  // namespace

uint16_t ReaderSession::clampProgress(const int progressBp) {
  return static_cast<uint16_t>(std::clamp(progressBp, 0, 10000));
}

void ReaderSession::noteTurn(const bool forward, const bool succeeded) { pendingForward = forward && succeeded; }

uint32_t ReaderSession::durationSeconds() const {
  // The journal stores whole seconds; round once per session, never per page.
  return pages ? static_cast<uint32_t>(std::clamp<uint64_t>((durationMs + 999) / 1000, 1, UINT32_MAX)) : 0;
}

void ReaderSession::onRenderComplete(const uint32_t monotonicMs, const int64_t trustedEpoch, const int progressBp) {
  const uint16_t progress = clampProgress(progressBp);
  if (pendingForward && hasAnchor && anchorEpoch > 0) {
    if (pages == 0) {
      startEpoch = anchorEpoch;
      startProgress = anchorProgress;
      endProgress = anchorProgress;
    }
    durationMs += std::min(monotonicMs - anchorMs, MAX_DWELL_MS);
    if (pages < UINT32_MAX) ++pages;
    endProgress = std::max(endProgress, progress);
  }

  anchorMs = monotonicMs;
  anchorEpoch = trustedEpoch;
  anchorProgress = progress;
  hasAnchor = true;
  pendingForward = false;
}

void ReaderSession::reset() {
  anchorMs = 0;
  anchorEpoch = 0;
  startEpoch = 0;
  durationMs = 0;
  pages = 0;
  anchorProgress = 0;
  startProgress = 0;
  endProgress = 0;
  hasAnchor = false;
  pendingForward = false;
}
