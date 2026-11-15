#pragma once

#include <HalMemory.h>

#include <atomic>

#include "../../activities/Activity.h"
#include "../ui/ToyboxScreen.h"
#include "BookPreparation.h"

// The loaded document remains owned by its reader on the activity stack.
// This child borrows it through Job, on the existing render task only.
class OpenedBookPreparationActivity final : public Activity {
 public:
  OpenedBookPreparationActivity(GfxRenderer& renderer, MappedInputManager& input, std::unique_ptr<bookprep::Job> job,
                                HalMemory::PsramBuffer background = {});
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  void runIdleWork() override;
  bool preventAutoSleep() override { return bookprep::running(phase.load()); }
  void prepareForSleep() override;
  bool handleHomeGesture() override;

 private:
  std::unique_ptr<bookprep::Job> job;
  // Temporary PSRAM-only backdrop, released with this modal. The job may
  // borrow the single framebuffer as decode scratch between progress paints.
  HalMemory::PsramBuffer backdrop;
  const GfxRenderer::Orientation bookOrientation;
  std::atomic<bookprep::Phase> phase{bookprep::Phase::Preparing};
  std::atomic<bool> cancel{false}, painted{false};
  toybox::Interactions interactions;
  unsigned long lastPaint = 0;
  char counts[48]{};
};

namespace bookprep {
void showOpeningProgress(GfxRenderer& renderer);
std::unique_ptr<Activity> onOpen(GfxRenderer& renderer, MappedInputManager& input, const std::shared_ptr<Epub>& book,
                                 HalMemory::PsramBuffer background = {});
std::unique_ptr<Activity> onOpen(GfxRenderer& renderer, MappedInputManager& input, const std::shared_ptr<Xtc>& book);
}  // namespace bookprep
