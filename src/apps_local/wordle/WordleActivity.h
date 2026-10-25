#pragma once
#include "../../activities/Activity.h"
#include "LocalWordle.h"
#include "WordleScreens.h"
class WordleActivity final : public Activity {
 public:
  WordleActivity(GfxRenderer& renderer, MappedInputManager& input) : Activity("Wordle", renderer, input) {}
  static std::unique_ptr<Activity> create(GfxRenderer& renderer, MappedInputManager& input);
  void onEnter() override;
  void onExit() override;
  void prepareForSleep() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  enum class View : uint8_t { Menu, Game, HowTo, Lengths, ConfirmNew };
  void save();
  void play(bool replace = false);
  void submit();
  localwordle::State state;
  localwordle::Store store;
  wordleui::Work work;
  wordleui::KeyboardLayout keys;
  toybox::Interactions interactions;
  View view = View::Menu;
  int pendingLetters = 5;
  const char* message = nullptr;
  uint32_t lastSave = 0;
  bool ready = false, dirty = false, saveFailed = false;
};
