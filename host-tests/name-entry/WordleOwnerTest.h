#pragma once

namespace owner_test {
inline bool lockAvailable = true;
class RenderLock {
 public:
  enum class Mode { Try, Blocking };
  explicit RenderLock(Mode mode) : owns_(lockAvailable) { assert(mode == Mode::Try); }
  bool ownsLock() const { return owns_; }

 private:
  bool owns_;
};
namespace shelf {
inline void leave(int, MappedInputManager&) { assert(false); }
}  // namespace shelf
struct WordleActivity {
  enum class View { Menu, Game };
  View view = View::Game;
  bool ready = true, localized = true, saveFailed = false;
  int renderer = 0, updates = 0, saves = 0;
  const char* message = nullptr;
  struct Native {
    bool over() const { return false; }
    bool submit(const char*) { return false; }
    bool persistenceFailed() const { return false; }
  } native;
  MappedInputManager mappedInput;
  EmbeddedKeyboard keyboard;
  void save() { ++saves; }
  void requestUpdate() { ++updates; }
  void loop();
};
PRODUCTION_WORDLE_LOOP
}  // namespace owner_test

static void wordleOwner() {
  owner_test::WordleActivity owner;
  assert(owner.keyboard.begin(Language::ES));
  owner.keyboard.wordGame(5);
  Trace target;
  fui::DeviceContext device;
  device.width = 480;
  device.height = 800;
  const auto box = fui::makeRect(12, 520, 456, 260);
  auto paint = [&] {
    target.labels.clear();
    owner.keyboard.drawGame(target, device, box, nullptr, nullptr);
    paintclock::notePainted();
    owner.keyboard.didPaint();
  };
  paint();
  const auto a = target.find("A");
  assert(!a.empty());
  owner.mappedInput.held = true;
  owner.mappedInput.x = a.x + a.width / 2;
  owner.mappedInput.y = a.y + a.height / 2;
  for (unsigned i = 0; i < 1000; ++i) owner.loop();
  assert(!*owner.keyboard.text());
  // Build, but do not paint the chooser yet.
  target.labels.clear();
  owner.keyboard.drawGame(target, device, box, nullptr, nullptr);
  const auto accent = target.find("Á");
  assert(!accent.empty());
  owner.mappedInput.held = false;
  owner.mappedInput.ended = true;
  owner.loop();
  // Input captured before paint must stay stale when the lock is unavailable.
  owner.mappedInput.ended = false;
  owner.mappedInput.tap = true;
  owner.mappedInput.x = accent.x + accent.width / 2;
  owner.mappedInput.y = accent.y + accent.height / 2;
  owner_test::lockAvailable = false;
  owner.loop();
  paintclock::notePainted();
  owner.keyboard.didPaint();
  owner_test::lockAvailable = true;
  owner.mappedInput.tap = false;
  owner.loop();
  assert(!*owner.keyboard.text());
  // Parent Back must dismiss the modal, preserving the current game/keyboard.
  owner.mappedInput.back = true;
  owner.loop();
  assert(owner.view == owner_test::WordleActivity::View::Game && owner.keyboard.available());
  assert(owner.saves == 0);
  owner.mappedInput.back = false;
  paint();
  owner.loop();
  owner.mappedInput.back = true;
  owner.loop();
  assert(owner.view == owner_test::WordleActivity::View::Menu && !owner.keyboard.available());
  std::puts("Production Wordle owner: pre-lock contact queue and modal Back ownership passed");
}
