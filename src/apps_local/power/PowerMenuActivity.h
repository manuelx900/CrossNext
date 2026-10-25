#pragma once
#include "../../activities/UiListActivity.h"
#include "../ui/PixelMenu.h"
namespace powermenu {
enum class Action : uint8_t { None, Sleep, Off, Restart };
Action take();
void request(Action action);
}  // namespace powermenu
class PowerMenuActivity final : public UiListActivity {
 public:
  PowerMenuActivity(GfxRenderer& r, MappedInputManager& i) : UiListActivity("PowerMenu", r, i, false, true) {}
  void onExit() override;

 private:
  powermenu::Action chosen = powermenu::Action::None;
  freeink::ui::ListItem items[3]{};
  pixelui::RowSource rows;
  int listCount() const override { return 3; }
  const char* headerTitle() const override;
  void buildScreen(UiScreen&) override;
  void activateIndex(int n) override;
};
