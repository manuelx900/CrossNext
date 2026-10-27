#pragma once
#include <functional>
#include <string>

#include "activities/Activity.h"
#include "components/OptionPopup.h"

class ConfirmationActivity : public Activity {
 private:
  // Input data
  std::string heading;
  std::string body;

  OptionPopup confirmPopup;
  bool acknowledgeOnly;

 public:
  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body, bool acknowledgeOnly = false);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
