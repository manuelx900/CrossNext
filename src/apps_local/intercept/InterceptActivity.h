#pragma once
#include "../../activities/Activity.h"
#include "InterceptScreens.h"
class InterceptActivity final : public Activity {
 public:
  InterceptActivity(GfxRenderer& r, MappedInputManager& i) : Activity("Intercept", r, i) {}
  static std::unique_ptr<Activity> create(GfxRenderer&, MappedInputManager&);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  intercept::State state;
  sdart::Pack assets;
  interceptui::View view = interceptui::View::Menu;
  interceptui::Work work;
  toybox::Interactions interactions;
  int selected = 0, page = 0;
  bool ready = false, writable = true, failed = false;
  void action(freeink::ui::ActionId, int);
  void back();
};
