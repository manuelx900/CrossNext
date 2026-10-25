#pragma once

#include <atomic>

#include "../../activities/Activity.h"
#include "FlashSettingsScreens.h"

class FlashSettingsActivity final : public Activity {
 public:
  FlashSettingsActivity(GfxRenderer& r, MappedInputManager& i, flashcards::TextApp app)
      : Activity("Card text settings", r, i) {
    model.app = app;
  }
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&,
                                          flashcards::TextApp = flashcards::TextApp::Flashcards);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  toybox::Interactions interactions;
  flashui::TextSettingsModel model;
  uint8_t originalQuestion = 0, originalAnswer = 0;
  bool originalSmart = true;
  std::atomic<bool> ready{false};
  bool saveChanges();
  void back();
};
