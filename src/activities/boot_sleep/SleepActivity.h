#pragma once

#include <ChromeRegion.h>
#include <HalMemory.h>

#include <optional>
#include <string>

#include "activities/Activity.h"
#include "apps_local/reading/ReadingGoal.h"

class Bitmap;
class Epub;
class HalFile;
struct Rect;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;
  bool showBookOrbitPreview(const Epub& book);
  void hideBookOrbitPopup();
  void restorePreviewFrame();

 private:
  void renderSleepScreen(bool fromReader, bool showEnteringPopup);
  void presentFrame(HalDisplay::RefreshMode refresh) const;
  bool capturePopupBackdrop(Rect rect) const;
  const std::string& randomSleepFile(bool transparent) const;

  // De-ghost the panel before painting a sleep image: HALF/FULL are both
  // single-shot absolute paints with no flush, so a mostly-black inverted sleep
  // image ghosts the prior high-contrast screen (a reader page) through. One
  // paint to white resets the baseline so the image paint lands clean (#2588).
  void flushPanelWhite() const;
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap, bool preserveBackground = false) const;
  bool renderSleepOverlayFile(HalFile& file, const char* pathForLog) const;
  bool renderTransparentOverlayPng(const std::string& path) const;
  bool renderSleepOverlayPath(const std::string& path) const;
  void renderLastScreenSleepScreen() const;
  void renderTransparentCustomSleepScreen() const;
  void renderBlankSleepScreen() const;
  void prepareGoalOverlay();
  void drawGoalOverlay() const;
  readinggoal::WallpaperData goalOverlay;

  mutable std::optional<std::string> selectedWallpaper;
  mutable std::optional<uint16_t> selectedWallpaperIndex;
  mutable HalMemory::PsramBuffer popupBackdrop;
  mutable GfxRenderer::Orientation popupOrientation = GfxRenderer::Orientation::Portrait;
  mutable int popupX = 0, popupY = 0, popupWidth = 0, popupHeight = 0;
  HalMemory::PsramBuffer prePreviewFrame;
  const Epub* previewBook = nullptr;
  GfxRenderer::Orientation prePreviewOrientation = GfxRenderer::Orientation::Portrait;
  int prePreviewHeaderHeight = 0;
  chrome::HeaderColor prePreviewHeaderColor = chrome::HeaderColor::Appearance;
  bool prePreviewInverted = false;
  bool prePreviewLightHeader = false;
  bool prePreviewPaintedHeader = false;
  bool showingBookOrbitPreview = false;
  bool fromTimeout = false;
};
