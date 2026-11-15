#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

#define CHECK(c)                                                \
  do {                                                          \
    if (!(c)) {                                                 \
      std::printf("Transition failure %d: %s\n", __LINE__, #c); \
      std::exit(1);                                             \
    }                                                           \
  } while (0)
#define LOG_DBG(...) ((void)0)
bool renderOwnsPixels = true, locked = false, released = false, pacePaused = false;
unsigned char pixel = 0x5a;
struct RenderLock {
  RenderLock() {
    // The current renderer completes before the pending push acquires its lock.
    CHECK(renderOwnsPixels && !released && pixel == 0x5a);
    renderOwnsPixels = false;
    locked = true;
  }
  ~RenderLock() { locked = false; }
};
struct ImageBlock {
  static void releaseRenderCache() {
    CHECK(locked && !renderOwnsPixels);
    released = true;
    pixel = 0;
  }
};
struct Activity {
  bool blockChildEdgeBack = true;
  virtual ~Activity() = default;
  virtual void prepareForChild() {}
  virtual void pauseReadingPace() {
    CHECK(locked && released);
    pacePaused = true;
  }
  void setEdgeBackEnabled(bool enabled, bool children) { CHECK(!enabled && children); }
  void onExit() {}
};
struct EpubReaderActivity : Activity {
  struct Resident {
    void clear() { CHECK(locked && !renderOwnsPixels); }
  } residentPage;
  void clearPrefetchedPage() { CHECK(locked && !renderOwnsPixels); }
  void prepareForChild() override;
};
#include "child-hook-under-test.inc"
struct HeaderBackTapTarget {
  static void clear() { CHECK(locked && released); }
};
enum class PendingAction { Replace, Push };
struct Manager {
  std::unique_ptr<Activity> currentActivity = std::make_unique<EpubReaderActivity>();
  std::unique_ptr<Activity> pendingActivity = std::make_unique<Activity>();
  std::vector<std::unique_ptr<Activity>> stackActivities;
  PendingAction pendingAction = PendingAction::Push;
  void exitActivity(RenderLock&) { CHECK(false); }
  void push() {
    stackActivities.reserve(2);
#include "child-transition-under-test.inc"
  }
};
int main() {
  Manager manager;
  manager.push();
  CHECK(released && pacePaused && !locked && !renderOwnsPixels);
  CHECK(!manager.currentActivity && manager.stackActivities.size() == 1);
  CHECK(manager.pendingActivity != nullptr);
  std::puts("Production child handoff waits for render ownership before releasing image payloads");
}
