#pragma once
#include <atomic>

#include "CrossPointSettings.h"
#include "activities/Activity.h"
#include "apps_local/ui/QuickSettingsScreen.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

// Top-anchored control center opened by a top-edge down-swipe,
// or a button bound to "Control Center" (iOS Control Center style): a grabber,
// the frontlight brightness/warmth sliders (on boards with a light), and a grid
// of quick-setting tiles — night mode, ghost-cleanup refresh, reading
// orientation, and Wi-Fi selection. The
// frontlight controls are always there: they are what the panel is for. Pure
// 1-bit: no dithered fills, selection reads as a filled tile. The grabber sits
// along the panel's bottom edge, the edge the sheet is dragged from.
class FrontlightPanelActivity final : public Activity, private UiAppHost {
  ButtonNavigator buttonNavigator;

  uint8_t brightness = 60;
  uint8_t warmth = 50;
  uint8_t rememberedWarmth = 50;
  bool lightOn = false;
  // lightOn is seeded from the live hardware state (Frontlight.isOn()), which
  // legitimately diverges from the saved SETTINGS.frontlightOn preference —
  // e.g. after a wake with frontlightRestoreOnWake off, the light stays off
  // live while the saved "was on" preference is deliberately kept (see
  // main.cpp's restoreLightOn). brightness/warmth have no such divergence
  // (always restored unconditionally on boot), so only lightOn needs a
  // touched-by-the-user flag: onExit() must not persist a mirror that never
  // reflected user intent in the first place.
  bool lightOnChanged = false;
  bool draggingSlider = false;
  bool suspendedForWifi = false;
  unsigned long lastClockPoll = 0;
  std::atomic<int> shownMinute{-1};
  int panelBottom = 0;
  int lastTouchX = -1, lastTouchY = -1;
  bool backdropPainted = false;

  // Quick-setting tiles, in grid order (2 columns): night mode, refresh,
  // orientation, Wi-Fi. Fixed set — shown on touch boards, absent elsewhere.
  static constexpr int kTileCount = 4;

  quickui::Work panelWork;
  freeink::ui::HeaderProps status;

  static void panelScreen(UiScreen& screen, void* user);
  static void onBrightnessEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onBrightnessStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onWarmthStepEvent(const freeink::ui::ActionEvent& event, void* user);
  static void onTileEvent(const freeink::ui::ActionEvent& event, void* user);

  void buildPanelScreen(UiScreen& screen);
  int computePanelBottom() const;
  void adjustBrightness(int delta);
  void adjustWarmth(int delta);
  void toggleLight();
  void toggleWarmth();
  void runTile(int idx);
  void syncLightState(bool useSavedWarmth = false);
  // Copy the panel's live brightness/warmth/lightOn into SETTINGS and save if
  // anything actually changed. onExit() runs it on every way out.
  void persistLightSettings();
  void close();

  // One-shot: a tile that rewrote the whole frame (night mode) re-drives it
  // with the ghost-cleanup waveform on the next render. The "refresh" tile does
  // not use this — it closes the panel and promotes the repaint underneath
  // instead (GfxRenderer::promoteNextRefresh).
  bool cleanRefreshPending = false;

 public:
  explicit FrontlightPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;
};
