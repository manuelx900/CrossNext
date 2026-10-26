#pragma once

#include "../../activities/Activity.h"
#include "ReadingProgressSync.h"
#include "ReadingStatsCloudClient.h"
#include "ReadingStatsScreens.h"
#include "ReadingStatsUpload.h"

class ReadingStatsSyncActivity final : public Activity {
 public:
  ReadingStatsSyncActivity(GfxRenderer& renderer, MappedInputManager& input)
      : Activity("ReadingStatsImport", renderer, input), client(&cancelRequest, this) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return result == readingstats::cloud::Result::Busy; }

 private:
  readingstats::cloud::Client client;
  std::unique_ptr<readingstats::cloud::Importer> importer;
  std::unique_ptr<readingstats::cloud::Uploader> uploader;
  std::unique_ptr<readingstats::cloud::ProgressSync> progress;
  enum class Phase { UploadStats, DownloadStats, Progress };
  Phase phase = Phase::UploadStats;
  readingstats::cloud::Result result = readingstats::cloud::Result::Idle;
  readingui::Work work;
  toybox::Interactions interactions;
  uint32_t books = 0;
  bool ownsWifi = false, waitingWifi = false, cancelFlag = false, ready = false;
  static bool cancelRequest(void* context);
  void start();
  void beginDownload();
  void done();
  void action(freeink::ui::ActionId action);
  const char* message() const;
};
