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
  std::string cancelLabel, confirmLabel;

 public:
  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body, bool acknowledgeOnly = false);

  ConfirmationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& heading,
                       const std::string& body, const std::string& cancelLabel, const std::string& confirmLabel);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&& lock) override;
};
