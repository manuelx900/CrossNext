#include "FrontlightPanelActivity.h"

#include <ChromeRegion.h>
#include <FreeInkUIIcon.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalFrontlight.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <iterator>

#include "../network/WifiSelectionActivity.h"
#include "../reader/ReaderUtils.h"
#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "apps_local/power/PowerMenuActivity.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "apps_local/ui/Rotation.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/icons/customListIcons.h"
#include "components/icons/listIcons.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_BRIGHTNESS = quickui::Brightness;
constexpr fui::ActionId ACTION_WARMTH = quickui::Warmth;
constexpr fui::ActionId ACTION_BRIGHTNESS_STEP = quickui::BrightnessStep;
constexpr fui::ActionId ACTION_WARMTH_STEP = quickui::WarmthStep;
constexpr fui::ActionId ACTION_TILE = quickui::Tile;  // value = tile index

// Buttons and physical keys use ten-point steps; zero switches the light off.
constexpr int BRIGHTNESS_STEP = 10;
constexpr uint8_t MIN_BRIGHTNESS = 0;

uint8_t percentFromPermille(const int16_t permille) {
  int value = (static_cast<int>(permille) * 100 + 500) / 1000;
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  return static_cast<uint8_t>(value);
}
}  // namespace

FrontlightPanelActivity::FrontlightPanelActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("FrontlightPanel", renderer, mappedInput), UiAppHost(renderer) {}

void FrontlightPanelActivity::onEnter() {
  Activity::onEnter();

  brightness = std::max(MIN_BRIGHTNESS, Frontlight.brightness());
  warmth = Frontlight.warmth();
  rememberedWarmth = warmth ? warmth : std::clamp<int>(SETTINGS.frontlightWarmthMemory, 1, 100);
  lightOn = Frontlight.isOn() && brightness > 0;
  lightOnChanged = false;

  resetUi();
  app.on(ACTION_BRIGHTNESS, &FrontlightPanelActivity::onBrightnessEvent, this);
  app.on(ACTION_WARMTH, &FrontlightPanelActivity::onWarmthEvent, this);
  app.on(ACTION_BRIGHTNESS_STEP, &FrontlightPanelActivity::onBrightnessStepEvent, this);
  app.on(ACTION_WARMTH_STEP, &FrontlightPanelActivity::onWarmthStepEvent, this);
  app.on(ACTION_TILE, &FrontlightPanelActivity::onTileEvent, this);
  app.setScreen(&FrontlightPanelActivity::panelScreen, this);
  requestUpdate();
}

void FrontlightPanelActivity::persistLightSettings() {
  // brightness/warmth are always restored unconditionally on boot (see
  // main.cpp), so they never diverge from SETTINGS at onEnter() — comparing
  // against SETTINGS here only fires on a genuine user change. lightOn has
  // no such guarantee (see lightOnChanged's declaration), so it's gated on
  // the user actually having touched it this session instead.
  const bool changed = SETTINGS.frontlightBrightness != brightness || SETTINGS.frontlightWarmth != warmth ||
                       SETTINGS.frontlightWarmthMemory != rememberedWarmth ||
                       (lightOnChanged && SETTINGS.frontlightOn != (lightOn ? 1 : 0));
  if (changed) {
    SETTINGS.frontlightBrightness = brightness;
    SETTINGS.frontlightWarmth = warmth;
    SETTINGS.frontlightWarmthMemory = rememberedWarmth;
    if (lightOnChanged) SETTINGS.frontlightOn = lightOn ? 1 : 0;
    SETTINGS.saveToFile();
  }
}

void FrontlightPanelActivity::onExit() {
  persistLightSettings();
  Activity::onExit();
}

void FrontlightPanelActivity::onBrightnessEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  if (event.dragPermille < 0) return;
  const auto value = percentFromPermille(event.dragPermille);
  if (value) {
    self->brightness = value;
    Frontlight.setBrightness(value);
  }
  if (self->lightOn != (value > 0)) {
    self->lightOn = value > 0;
    self->lightOnChanged = true;
    Frontlight.setOn(self->lightOn);
  }
}

void FrontlightPanelActivity::onWarmthEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<FrontlightPanelActivity*>(user);
  if (event.dragPermille < 0) return;
  self->warmth = percentFromPermille(event.dragPermille);
  if (self->warmth) self->rememberedWarmth = self->warmth;
  Frontlight.setWarmth(self->warmth);
  if (self->warmth && !self->lightOn) self->toggleLight();
}

void FrontlightPanelActivity::onBrightnessStepEvent(const fui::ActionEvent& event, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->adjustBrightness(event.value);
}

void FrontlightPanelActivity::onWarmthStepEvent(const fui::ActionEvent& event, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->adjustWarmth(event.value);
}

void FrontlightPanelActivity::onTileEvent(const fui::ActionEvent& event, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->runTile(event.value);
}

void FrontlightPanelActivity::runTile(const int idx) {
  app.clearTapFlash();
  if (idx < 0) {
    close();
    return;
  }
  switch (static_cast<quickui::Shortcut>(idx)) {
    case quickui::Shortcut::Dark:  // Dark mode (inverted output polarity, applied to the whole UI)
      SETTINGS.screenInverted = SETTINGS.screenInverted ? 0 : 1;
      SETTINGS.saveToFile();
      // Inversion rewrites every pixel; take the clean waveform so the panel
      // does not keep a ghost of the old polarity.
      cleanRefreshPending = true;
      requestUpdate();
      break;
    case quickui::Shortcut::Refresh:  // Ghost-cleanup refresh of the whole frame
      // Refreshing with the panel still up would clean a frame the user is
      // about to dismiss anyway: drop the panel first and let the repaint of
      // the screen underneath carry the clean waveform instead.
      renderer.promoteNextRefresh(HalDisplay::FULL_REFRESH);
      close();
      break;
    case quickui::Shortcut::Rotate:
      SETTINGS.orientation = rotation::pair(int(renderer.getOrientation()), SETTINGS.rotationDirection != 0);
      SETTINGS.saveToFile();
      {
        RenderLock lock;
        ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);
        renderer.clearScreen();
        panelBottom = 0;
        resetUi();
      }
      cleanRefreshPending = true;
      requestUpdate();
      break;
    case quickui::Shortcut::Light:
      toggleLight();
      break;
    case quickui::Shortcut::Warm:
      toggleWarmth();
      break;
    case quickui::Shortcut::Wifi:
      persistLightSettings();
      if (auto next =
              makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, false, WifiSelectionPurpose::Manage))
        startActivityForResult(std::move(next), [this](const ActivityResult&) { requestUpdate(); });
      else
        LOG_ERR("QUICK", "OOM: Wi-Fi selection");
      break;
    case quickui::Shortcut::Lock:
    case quickui::Shortcut::Sleep:
      persistLightSettings();
      powermenu::request(powermenu::Action::Sleep);
      close();
      return;
    case quickui::Shortcut::Off:
      persistLightSettings();
      powermenu::request(powermenu::Action::Off);
      close();
      return;
    case quickui::Shortcut::Restart:
      persistLightSettings();
      powermenu::request(powermenu::Action::Restart);
      close();
      return;
    case quickui::Shortcut::Count:
      break;
  }
}

void FrontlightPanelActivity::adjustBrightness(const int delta) {
  const int current = lightOn ? brightness : 0;
  int next = current + delta;
  if (next < MIN_BRIGHTNESS) next = MIN_BRIGHTNESS;
  if (next > 100) next = 100;
  if (next == current) return;
  if (next) {
    brightness = static_cast<uint8_t>(next);
    Frontlight.setBrightness(brightness);
  }
  if (lightOn != (next > 0)) {
    lightOn = next > 0;
    lightOnChanged = true;
    Frontlight.setOn(lightOn);
  }
  requestUpdate();
}

void FrontlightPanelActivity::adjustWarmth(const int delta) {
  const int current = lightOn ? warmth : 0;
  int next = current + delta;
  if (next < 0) next = 0;
  if (next > 100) next = 100;
  if (next == current) return;
  warmth = static_cast<uint8_t>(next);
  if (warmth) rememberedWarmth = warmth;
  Frontlight.setWarmth(warmth);
  if (warmth && !lightOn) toggleLight();
  requestUpdate();
}

void FrontlightPanelActivity::toggleLight() {
  app.clearTapFlash();
  lightOn = !lightOn;
  if (lightOn && brightness == 0) {
    brightness = BRIGHTNESS_STEP;
    Frontlight.setBrightness(brightness);
  }
  lightOnChanged = true;
  Frontlight.setOn(lightOn);
  requestUpdate();
}

void FrontlightPanelActivity::toggleWarmth() {
  app.clearTapFlash();
  if (!Frontlight.hasColorTemperature()) return;
  if (lightOn && warmth) {
    rememberedWarmth = warmth;
    warmth = 0;
  } else {
    warmth = rememberedWarmth;
    if (!lightOn) toggleLight();
  }
  Frontlight.setWarmth(warmth);
  requestUpdate();
}
void FrontlightPanelActivity::close() { finish(); }

bool FrontlightPanelActivity::handleHomeGesture() {
  close();
  return true;
}

void FrontlightPanelActivity::loop() {
  const auto touch = routeTouch(mappedInput, false, /*routeHeld=*/true);
  if (touch.routed) {
    if (app.invalidated()) requestUpdate();
    if (touch) {
      if (touch.event.dragPermille >= 0) draggingSlider = true;
      return;
    }
    // Swipe up dismisses the sheet, the way it was pulled down from the top
    // edge. draggingSlider keeps a fast slider flick from closing it.
    if (!draggingSlider && mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Up) {
      close();
      return;
    }
    // panelBottom > 0 guards the frame the sheet opens in: the release that
    // opened it (a status-bar tap) is still in the input snapshot when the panel
    // runs its first loop(), and panelBottom is only known once render() has
    // measured the layout — so at 0 that release read as "tapped below the
    // sheet" and closed it again before it was ever drawn.
    if (touch.snap.touchReleased && !draggingSlider && panelBottom > 0 && touch.snap.touchY >= panelBottom) {
      close();
      return;
    }
  }
  if (draggingSlider) {
    if (!touch.snap.touchHeld) {
      draggingSlider = false;
      cleanRefreshPending = true;
      requestUpdate();
    }
    return;
  }

  if (!touch.snap.touchHeld && millis() - lastClockPoll >= 1000) {
    lastClockPoll = millis();
    tm local{};
    const auto now = std::time(nullptr);
    const bool valid = (now > 0 && localtime_r(&now, &local) && local.tm_year >= 120) || halClock.localTime(local);
    const int minute = valid ? local.tm_hour * 60 + local.tm_min : -1;
    if (minute != shownMinute) {
      shownMinute = minute;
      requestUpdate();
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    close();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    toggleLight();
    return;
  }

  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Left},
                                       [this] { adjustBrightness(-BRIGHTNESS_STEP); });
  buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Right},
                                       [this] { adjustBrightness(BRIGHTNESS_STEP); });
}

int FrontlightPanelActivity::computePanelBottom() const {
  return quickui::layout(uiTarget.deviceContext(), Frontlight.present(), Frontlight.hasColorTemperature(),
                         mappedInput.hasTouch())
      .bottom;
}
void FrontlightPanelActivity::panelScreen(UiScreen& screen, void* user) {
  static_cast<FrontlightPanelActivity*>(user)->buildPanelScreen(screen);
}
void FrontlightPanelActivity::buildPanelScreen(UiScreen& screen) {
  chrome::headerHeight = 0;
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  status.status.battery.label = nullptr;
  GUI.applyHeaderStatus(renderer, status);
  quickui::Model model;
  tm local{};
  const auto now = std::time(nullptr);
  if ((now > 0 && localtime_r(&now, &local) && local.tm_year >= 120) || halClock.localTime(local))
    std::snprintf(model.time, sizeof(model.time), "%02d:%02d", local.tm_hour, local.tm_min);
  std::copy(std::begin(SETTINGS.quickActions), std::end(SETTINGS.quickActions), model.shortcuts);
  model.wifiConnected = WiFi.status() == WL_CONNECTED;
  model.brightness = lightOn ? brightness : 0;
  model.warmth = lightOn ? warmth : 0;
  model.rotateRight = SETTINGS.rotationDirection != 0;
  model.battery = status.status.battery.percent;
  model.charging = status.status.battery.charging;
  model.light = Frontlight.present();
  model.temperature = Frontlight.hasColorTemperature();
  model.night = SETTINGS.screenInverted;
  model.touch = mappedInput.hasTouch();
  model.touchOn = !mappedInput.isTouchscreenLocked();
  model.lightOn = lightOn;
  quickui::build(screen, model, panelWork);
}

void FrontlightPanelActivity::render(RenderLock&&) {
  panelBottom = computePanelBottom();

  // fui::sheet draws the card body, its bottom rule, and the grabber during
  // renderUi(); the battery band at the card's top is part of the screen build.
  renderUi();

  // A tile that rewrote the whole frame (night mode) re-drives every pixel
  // once; ordinary repaints stay on the fast path. HALF: strong enough to
  // flip the whole frame's polarity without the FULL waveform's blackout
  // flash. Any faint residue clears with the panel's dedicated refresh tile
  // or the next scheduled clean refresh.
  renderer.displayBuffer(cleanRefreshPending ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanRefreshPending = false;
}
