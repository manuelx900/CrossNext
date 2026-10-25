#pragma once

#include <functional>
#include <string>

#include "MappedInputManager.h"
#include "activities/Activity.h"

class BmpViewerActivity final : public Activity {
 public:
  BmpViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string filePath);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void prepareForSleep() override { flushImageStats(); }

 private:
  std::string trackedImage;
  uint32_t statsStartedMs = 0;
  int64_t statsEpoch = 0;
  void beginImageStats();
  void flushImageStats();
  void loadSiblingImages();
  void doSetSleepCover();
  bool canSetSleepCover() const;
  bool renderImage();
  bool saveJpegSleepCover();

  std::string filePath;
  std::vector<std::string> siblingImages;
  int currentImageIndex = -1;
};
