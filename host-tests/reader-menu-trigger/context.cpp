#include <atomic>
#include <cassert>
#include <cstdio>
#include <memory>
class Activity {
 public:
  virtual ~Activity() = default;
  virtual bool isReaderActivity() const { return false; }
  virtual bool acceptsReaderTextInput() const { return false; }
};
class ClipSelectionActivity final : public Activity {
 public:
  bool isReaderActivity() const override { return true; }
};
class ReaderActivity final : public Activity {
 public:
  std::atomic<bool> pageRendered{false};
  bool overlay = false;
  bool isReaderActivity() const override { return true; }
  bool hasReadingOverlay() const { return overlay; }
  bool acceptsReaderTextInput() const final;
};
class ActivityManager {
 public:
  enum class PendingAction { None, Push };
  PendingAction pendingAction = PendingAction::None;
  std::unique_ptr<Activity> currentActivity;
  bool readerTextInputActive() const;
};
#include "reader-context-production.inc"
int main() {
  ActivityManager manager;
  assert(!manager.readerTextInputActive());
  manager.currentActivity = std::make_unique<ClipSelectionActivity>();
  assert(manager.currentActivity->isReaderActivity() && !manager.readerTextInputActive());
  auto reader = std::make_unique<ReaderActivity>();
  auto* document = reader.get();
  manager.currentActivity = std::move(reader);
  assert(!manager.readerTextInputActive());
  document->pageRendered = true;
  assert(manager.readerTextInputActive());
  document->overlay = true;
  assert(!manager.readerTextInputActive());
  document->overlay = false;
  manager.pendingAction = ActivityManager::PendingAction::Push;
  assert(!manager.readerTextInputActive());
  std::puts(
      "Production reader eligibility: selection context, document readiness, XTC-style overlay and pending navigation "
      "passed");
}
