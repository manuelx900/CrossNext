#pragma once

#include <GfxRenderer.h>

#include "activities/UiListActivity.h"
#include "activities/util/KeyboardLayoutSet.h"

class MappedInputManager;

class KeyboardLayoutsActivity final : public UiListActivity {
 public:
  explicit KeyboardLayoutsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("KeyboardLayouts", renderer, mappedInput, false, true) {}

  void onEnter() override;
  void onExit() override;

 private:
  int listCount() const override { return rowCount; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  bool isLocked(uint8_t i) const;
  bool checked(uint8_t i) const;
  static void provideRow(void* context, uint16_t index, freeink::ui::ListItem& item);
  bool handleCustomInput() override;
  bool isAvailable(uint8_t index) const;

  Language languages[unsigned(Language::_COUNT)]{};
  uint8_t rowCount = 0;
  uint64_t installed = 0;
  uint64_t workingLanguages = 0;
  uint16_t workingMask = 0;
  // Do not persist a derived default merely because the screen was visited.
  bool edited = false;
};
