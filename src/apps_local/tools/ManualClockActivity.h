#pragma once
#include <ctime>

#include "../ui/ToyboxScreen.h"
#include "activities/Activity.h"

class ManualClockActivity final : public Activity {
 public:
  ManualClockActivity(GfxRenderer& r, MappedInputManager& i) : Activity("ManualClock", r, i) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  void step(int field, int delta);
  void commit();
  struct tm value{};
  int selected = 0;
  bool ready = false, failed = false;
  toybox::Interactions interactions;
};
