#include <atomic>
#include <cassert>
#include <cstdio>
#include <initializer_list>

#include "src/apps_local/readertext/ReaderMenuTrigger.h"
#include "src/util/HomeButtonInput.h"
struct Settings {
  uint8_t readerTextTrigger = 0;
  bool touchReaderControls = true;
} settings;
#define SETTINGS settings
struct RenderLock {
  static inline int held = 0;
  bool owned = true;
  template <class T>
  explicit RenderLock(T&) {
    assert(held == 0);
    ++held;
  }
  ~RenderLock() {
    if (owned) --held;
  }
  void unlock() {
    assert(owned);
    owned = false;
    --held;
  }
};
class ReaderActivity {
 public:
  struct Input {
    HomeButtonAction action = HomeButtonAction::Ignore;
    bool edge = false, tap = false;
    int x = 240, y = 400;
    HomeButtonAction homeButtonAction() const { return action; }
    bool wasReaderTextSettingsGesture() const { return edge || action == HomeButtonAction::ReaderMenu; }
    bool wasScreenTapped(int& px, int& py) const {
      px = x;
      py = y;
      return tap;
    }
  } mappedInput;
  struct Renderer {
    int getScreenWidth() const { return 480; }
    int getScreenHeight() const { return 800; }
  } renderer;
  std::atomic<bool> pageRendered{true};
  bool overlay = false, linked = false;
  int opened = 0;
  bool inputOverPage() const { return false; }
  bool hasReadingOverlay() const { return overlay; }
  bool readerMenuTapAllowed(int, int) const { return !linked; }
  void openTextSettings() {
    RenderLock lock(*this);
    ++opened;
  }
  bool handleReaderTextSettingsGesture();
};
#include "reader-dispatch-production.inc"
int main() {
  ReaderActivity reader;
  reader.mappedInput.edge = true;
  assert(reader.handleReaderTextSettingsGesture() && reader.opened == 1);
  settings.readerTextTrigger = uint8_t(readertext::MenuTrigger::CenterTap);
  assert(reader.handleReaderTextSettingsGesture() && reader.opened == 1);
  reader.mappedInput.edge = false;
  reader.mappedInput.tap = true;
  assert(reader.handleReaderTextSettingsGesture() && reader.opened == 2);
  reader.linked = true;
  assert(!reader.handleReaderTextSettingsGesture() && reader.opened == 2);
  reader.linked = false;
  settings.touchReaderControls = false;
  assert(!reader.handleReaderTextSettingsGesture());
  reader.mappedInput.action = HomeButtonAction::ReaderMenu;
  assert(reader.handleReaderTextSettingsGesture() && reader.opened == 3);
  reader.overlay = true;
  assert(reader.handleReaderTextSettingsGesture() && reader.opened == 3);
  reader.mappedInput.action = HomeButtonAction::Ignore;
  settings.touchReaderControls = true;
  assert(!reader.handleReaderTextSettingsGesture() && reader.opened == 3);
  reader.mappedInput.action = HomeButtonAction::ReaderMenu;
  reader.overlay = false;
  reader.pageRendered = false;
  assert(reader.handleReaderTextSettingsGesture() && reader.opened == 3);
  reader.mappedInput.action = HomeButtonAction::Ignore;
  reader.pageRendered = true;
  reader.mappedInput.tap = true;
  for (auto mode : {readertext::MenuTrigger::TopTap, readertext::MenuTrigger::BottomTap}) {
    settings.readerTextTrigger = uint8_t(mode);
    reader.mappedInput.y = mode == readertext::MenuTrigger::TopTap ? 12 : 790;
    const int before = reader.opened;
    assert(reader.handleReaderTextSettingsGesture() && reader.opened == before + 1);
    reader.mappedInput.y = 400;
    assert(!reader.handleReaderTextSettingsGesture() && reader.opened == before + 1);
    reader.mappedInput.y = mode == readertext::MenuTrigger::TopTap ? 12 : 790;
    reader.overlay = true;
    assert(!reader.handleReaderTextSettingsGesture() && reader.opened == before + 1);
    reader.overlay = false;
    reader.linked = true;
    assert(!reader.handleReaderTextSettingsGesture() && reader.opened == before + 1);
    reader.linked = false;
    settings.touchReaderControls = false;
    assert(!reader.handleReaderTextSettingsGesture() && reader.opened == before + 1);
    settings.touchReaderControls = true;
  }
  std::puts(
      "Production reader dispatch: swipe default, selected center, links, disabled touch, dedicated key, overlay and "
      "ready gate passed");
}
