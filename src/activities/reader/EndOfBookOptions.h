#pragma once

#include <atomic>

#include "activities/Activity.h"
#include "components/UiAppHost.h"

class GfxRenderer;
class MappedInputManager;

// Shared end screen for the EPUB and XTC readers. UiAppHost owns touch
// publication; loadOnce() release-publishes setup to the reader's input task.
class EndOfBookOptions : private UiAppHost {
 public:
  enum class Action { None, Redraw, GoHome, LastPage };

  explicit EndOfBookOptions(GfxRenderer& renderer);
  void loadOnce();
  bool menuActive() const;
  Action handleMenuInput(const MappedInputManager& input);
  void render();

 private:
  static void endScreen(UiScreen& screen, void* user);
  static void onAction(const freeink::ui::ActionEvent& event, void* user);

  std::atomic<bool> isLoaded{false};
  Action pendingAction = Action::None;
};
