#include "StatusBattery.h"
#pragma once

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <components/bars/battery-indicator.h>

#include <algorithm>
#include <cstdio>

#include "BluetoothState.h"
#include "FitText.h"
#include "PixelMenu.h"
#include "QuickAction.h"
#include "SettingsOptionIcons.h"

namespace quickui {
namespace fui = freeink::ui;
inline fui::Rect intersect(fui::Rect a, fui::Rect b) {
  const int left = std::max<int>(a.x, b.x), top = std::max<int>(a.y, b.y);
  return fui::makeRect(left, top, std::max(0, std::min<int>(a.right(), b.right()) - left),
                       std::max(0, std::min<int>(a.bottom(), b.bottom()) - top));
}
inline void styleConfiguration(fui::ListProps& props, bool checkboxRows = true) {
  props.rowStyles.selected = props.rowStyles.normal;
  props.rowStyles.focused = props.rowStyles.normal;
  props.rowStyles.active = props.rowStyles.normal;
  props.selectionMarker = checkboxRows ? fui::SelectionMarker::None : fui::SelectionMarker::Underline;
  props.markerThickness = 2;
  props.toggleHeight = props.toggleWidth;
  props.toggleCheckbox = true;
  props.checkboxCheckmark = true;
}
inline const char* label(Shortcut shortcut, bool lightOn = true, bool warmOn = true) {
  switch (shortcut) {
    case Shortcut::Dark:
      return tr(STR_NIGHT_MODE);
    case Shortcut::Refresh:
      return tr(STR_QUICK_REFRESH);
    case Shortcut::Rotate:
      return tr(STR_QUICK_ROTATE);
    case Shortcut::Light:
      return lightOn ? tr(STR_QUICK_LIGHT_OFF) : tr(STR_QUICK_LIGHT_ON);
    case Shortcut::Warm:
      return warmOn ? tr(STR_QUICK_WARM_OFF) : tr(STR_QUICK_WARM_ON);
    case Shortcut::Wifi:
      return tr(STR_QUICK_WIFI);
    case Shortcut::Bluetooth:
      return tr(STR_BT_SHORT_NAME);
    case Shortcut::Lock:
      return tr(STR_QUICK_LOCK);
    case Shortcut::Sleep:
      return tr(STR_POWER_SLEEP);
    case Shortcut::Off:
      return tr(STR_POWER_OFF);
    case Shortcut::Restart:
      return tr(STR_POWER_RESTART);
    case Shortcut::Count:
      break;
  }
  return "";
}
inline const freeink::Icon& icon(Shortcut shortcut, bool on = true, bool right = false) {
  static constexpr freeink::Icon moon{64, 64, 32, settingsicons::QuickMoonBits};
  static constexpr freeink::Icon refresh{64, 64, 32, settingsicons::QuickRefreshBits};
  static constexpr freeink::Icon rotate{64, 64, 32, settingsicons::QuickRotateBits};
  static constexpr freeink::Icon rotateRight{64, 64, 32, settingsicons::QuickRotateRightBits};
  static constexpr freeink::Icon light{64, 64, 32, settingsicons::QuickLightBits};
  static constexpr freeink::Icon lightOff{64, 64, 32, settingsicons::QuickLightOffBits};
  static constexpr freeink::Icon warm{64, 64, 32, settingsicons::QuickWarmBits};
  static constexpr freeink::Icon warmOff{64, 64, 32, settingsicons::QuickWarmOffBits};
  static constexpr freeink::Icon wifi{64, 64, 32, settingsicons::QuickWifiBits};
  static constexpr freeink::Icon bluetooth{64, 64, 32, settingsicons::QuickBluetoothBits};
  static constexpr freeink::Icon lock{64, 64, 32, settingsicons::QuickLockBits};
  static constexpr freeink::Icon power{64, 64, 32, settingsicons::QuickPowerBits};
  switch (shortcut) {
    case Shortcut::Dark:
    case Shortcut::Sleep:
      return moon;
    case Shortcut::Refresh:
    case Shortcut::Restart:
      return refresh;
    case Shortcut::Rotate:
      return right ? rotateRight : rotate;
    case Shortcut::Light:
      return on ? light : lightOff;
    case Shortcut::Warm:
      return on ? warm : warmOff;
    case Shortcut::Wifi:
      return wifi;
    case Shortcut::Bluetooth:
      return bluetooth;
    case Shortcut::Lock:
      return lock;
    case Shortcut::Off:
      return power;
    case Shortcut::Count:
      break;
  }
  return light;
}
enum : fui::ActionId {
  Brightness = 1,
  Warmth,
  ToggleLight,
  BrightnessStep,
  WarmthStep,
  Tile,
  ToggleWarmth,
  OpenSettings
};
enum class SettingsDestination : int16_t { Bluetooth, Wifi };
struct Model {
  char time[8] = "--:--";
  int brightness = 60, warmth = 50, battery = 100;
  uint8_t shortcuts[4] = {0, 1, 2, 3};
  bluetoothui::State bluetooth = bluetoothui::State::Off;
  bool wifiConnected = false, charging = false, shadeBackdrop = true;
  bool rotateRight = false;
  bool light = true, temperature = true, night = false, touch = true, touchOn = true, lightOn = true;
};
struct Work {
  fui::ButtonProps button;
  fui::SliderProps slider;
  char label[96]{};
};
struct Layout {
  fui::Rect header, tiles, brightness, warmth;
  int bottom = 0;
};
inline bool hasBluetoothStatus(bluetoothui::State state) {
  return state != bluetoothui::State::Off && state != bluetoothui::State::Unavailable;
}
struct BluetoothSpacing {
  int banner, gap, tilesGap, sectionGap, rowGap, footer;
};
inline constexpr BluetoothSpacing kBluetoothSpacing{56, 8, 16, 24, 24, 54};
inline int headerBarHeight(const fui::DeviceContext& device, bool bluetooth) {
  return chrome::lightHeader || (bluetooth && device.width > device.height) ? 44 : (bluetooth ? 56 : 66);
}
inline Layout layout(const fui::DeviceContext& device, bool light, bool temperature, bool touch,
                     bool bluetooth = false) {
  const auto safe = device.safeRect();
  const bool wide = device.width > device.height;
  const int x = safe.x + 18, width = safe.width - 36;
  Layout out;
  const int headerHeight =
      headerBarHeight(device, bluetooth) + (bluetooth ? kBluetoothSpacing.gap + kBluetoothSpacing.banner : 0);
  out.header = fui::makeRect(x, safe.y + 4, width, headerHeight);
  if (chrome::lightHeader) {
    int y = out.header.bottom() + (bluetooth ? 20 : 6);
    if (light) {
      const int rowWidth = wide && temperature ? (width - 24) / 2 : width;
      out.brightness = fui::makeRect(x, y, rowWidth, 92);
      if (temperature)
        out.warmth = wide ? fui::makeRect(x + rowWidth + 24, y, rowWidth, 92)
                          : fui::makeRect(x, y + 92 + (bluetooth ? kBluetoothSpacing.rowGap : 12), rowWidth, 92);
      y = temperature ? out.warmth.bottom() : out.brightness.bottom();
    }
    out.tiles = fui::makeRect(x, y + (bluetooth ? 20 : 14), width, touch ? (bluetooth ? 104 : 96) : 0);
    out.bottom = std::min<int>(safe.bottom(), out.tiles.bottom() + (bluetooth ? kBluetoothSpacing.footer : 46));
    return out;
  }
  out.tiles = fui::makeRect(x, out.header.bottom() + (bluetooth ? (wide ? 12 : kBluetoothSpacing.tilesGap) : 10), width,
                            touch ? (wide ? (bluetooth ? 120 : 128) : 152) : 0);
  int y = out.tiles.bottom() + (touch ? (bluetooth ? (wide ? 16 : kBluetoothSpacing.sectionGap) : 24) : 8);
  if (light) {
    const int rowW = wide && temperature ? (width - 24) / 2 : width;
    out.brightness = fui::makeRect(x, y, rowW, 120);
    if (temperature)
      out.warmth = wide ? fui::makeRect(x + rowW + 24, y, rowW, 120)
                        : fui::makeRect(x, y + 120 + (bluetooth ? kBluetoothSpacing.rowGap : 16), rowW, 120);
    y = temperature ? out.warmth.bottom() : out.brightness.bottom();
  }
  out.bottom = std::min<int>(safe.bottom(), y + (bluetooth ? kBluetoothSpacing.footer : 54));
  return out;
}
template <typename Screen>
void button(Screen& screen, Work& work, fui::Rect rect, const char* label, fui::ActionId action, int value = 0) {
  work.button = {};
  auto& p = work.button;
  p.styles = chrome::lightHeader ? lightui::controlStyles(true) : pixelui::buttonStyles();
  p.action = action;
  p.value = value;
  p.label = label;
  p.text.font = chrome::lightHeader ? pixelui::kBody : pixelui::kSmall;
  p.text.align = fui::TextAlign::Center;
  p.minTouchSize = 0;
  screen.button(p, rect);
}
template <typename Screen>
void slider(Screen& screen, const Model& model, Work& work, fui::Rect box, bool warmth) {
  const int value = warmth ? model.warmth : model.brightness;
  if (chrome::lightHeader) {
    std::snprintf(work.label, sizeof(work.label), "%s: %d%%", warmth ? tr(STR_WARMTH) : tr(STR_BRIGHTNESS), value);
    pixelui::text(screen.target(), fui::makeRect(box.x, box.y, box.width, 36), work.label, 0);
    const int y = box.y + 38, step = 52;
    button(screen, work, fui::makeRect(box.x, y, step, step), "-", warmth ? WarmthStep : BrightnessStep, -10);
    button(screen, work, fui::makeRect(box.right() - step, y, step, step), "+", warmth ? WarmthStep : BrightnessStep,
           10);
    const auto action = warmth ? Warmth : Brightness;
    const auto band = fui::makeRect(box.x + 62, y, box.width - 124, 52);
    const auto track = fui::makeRect(band.x, y + 12, band.width, 28);
    screen.target().fill(track, lightui::paper, 4);
    const int filled = (track.width - 2) * value / 100;
    if (filled) {
      const auto previousClip = screen.target().clipRect();
      screen.target().setClipRect(
          intersect(previousClip, fui::makeRect(track.x + 1, track.y + 1, filled, track.height - 2)));
      screen.target().fill(track.inset({1, 1, 1, 1}), lightui::ink, 3);
      screen.target().setClipRect(previousClip);
    }
    screen.target().stroke(track, lightui::ink, 1, 4);
    for (int n = 1; n < 10; ++n)
      screen.target().fill(fui::makeRect(track.x + track.width * n / 10, track.y + 7, 1, 14),
                           n * 10 <= value ? lightui::paper : lightui::ink);
    screen.frame().hit(band, action, value, fui::InputTouch | fui::InputDrag);
    return;
  }
  const auto& controlIcon = icon(warmth ? Shortcut::Warm : Shortcut::Light, warmth ? model.warmth > 0 : model.lightOn);
  screen.target().bitmap(fui::makeRect(box.x, box.y, 44, 44), fui::bitmapFromIcon(controlIcon),
                         fui::BitmapMode::Contain);
  pixelui::copyCaps(work.label, sizeof(work.label), warmth ? tr(STR_WARMTH) : tr(STR_BRIGHTNESS));
  pixelui::text(screen.target(), fui::makeRect(box.x + 44, box.y, box.width - 118, 40), work.label, pixelui::kSmall);
  std::snprintf(work.label, sizeof(work.label), "%d%%", value);
  pixelui::text(screen.target(), fui::makeRect(box.right() - 74, box.y, 74, 40), work.label, pixelui::kSmall,
                fui::Color::Black, fui::TextAlign::Right);
  const int y = box.y + 54, step = 54;
  const auto action = warmth ? WarmthStep : BrightnessStep;
  button(screen, work, fui::makeRect(box.x, y, step, step), "-", action, -10);
  button(screen, work, fui::makeRect(box.right() - step, y, step, step), "+", action, 10);
  work.slider = {};
  auto& p = work.slider;
  p.value = value;
  p.action = warmth ? Warmth : Brightness;
  p.inputMask = fui::InputTouch | fui::InputDrag;
  p.track = fui::Paint::solid(fui::Color::White);
  p.fill = fui::Paint::solid(fui::Color::Black);
  p.knob = fui::Paint::solid(fui::Color::White);
  p.knobWidth = 22;
  p.knobHeight = 44;
  p.trackHeight = 10;
  p.horizontalPadding = 0;
  p.minTouchSize = 0;
  const auto track = fui::makeRect(box.x + step + 12, y, box.width - 2 * step - 24, step);
  fui::slider(screen.frame(), track, p);
  screen.target().stroke(fui::makeRect(track.x, y + (step - 10) / 2, track.width, 10),
                         fui::Paint::solid(fui::Color::Black), 2);
  const int knobX = track.x + (track.width - p.knobWidth) * value / 100;
  const auto knob = fui::makeRect(knobX, y + (step - p.knobHeight) / 2, p.knobWidth, p.knobHeight);
  screen.target().fill(knob, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(knob, fui::Paint::solid(fui::Color::Black), 3);
  {
    work.button = {};
    work.button.styles.explicitlySet = true;
    work.button.action = Tile;
    work.button.value = static_cast<int16_t>(warmth ? Shortcut::Warm : Shortcut::Light);
    work.button.minTouchSize = 0;
    screen.button(work.button, fui::makeRect(box.x, box.y, 44, 44));
  }
}
template <typename Screen>
void build(Screen& screen, const Model& model, Work& work) {
  const bool bluetooth = hasBluetoothStatus(model.bluetooth);
  const auto g = layout(screen.device(), model.light, model.temperature, model.touch, bluetooth);
  const auto bar = fui::makeRect(g.header.x, g.header.y, g.header.width, headerBarHeight(screen.device(), bluetooth));
  fui::SheetProps sheet;
  sheet.radius = 0;
  sheet.ruleWidth = chrome::lightHeader ? 1 : pixelui::kBorder;
  sheet.grabberHeight = sheet.grabberInset = sheet.grabberMargin = 0;
  if (chrome::lightHeader && model.shadeBackdrop)
    lightui::shade(screen.target(),
                   fui::makeRect(0, g.bottom, screen.device().width, screen.device().height - g.bottom));
  screen.sheet(sheet, g.bottom);
  pixelui::copyCaps(work.label, sizeof(work.label), chrome::lightHeader ? "" : tr(STR_SETTINGS_TITLE));
  pixelui::text(screen.target(), fui::makeRect(bar.x, bar.y, bar.width / 3, bar.height), work.label, pixelui::kSmall);
  const auto battery =
      fui::makeRect(g.header.right() - statusbattery::width - statusbattery::terminal,
                    bar.y + (bar.height - statusbattery::height) / 2, statusbattery::width, statusbattery::height);
  const auto wifi = fui::makeRect(battery.x - 126, bar.y, 44, bar.height);
  auto clock = fui::makeRect(chrome::lightHeader ? bar.x : bar.x + bar.width / 3, bar.y, bar.width / 3, bar.height);
  if (model.wifiConnected) clock.width = std::min<int>(clock.width, wifi.x - 8 - clock.x);
  pixelui::text(screen.target(), clock, model.time, pixelui::kSmall, fui::Color::Black,
                chrome::lightHeader ? fui::TextAlign::Left : fui::TextAlign::Center);
  std::snprintf(work.label, sizeof(work.label), "%d%%", model.battery);
  pixelui::text(screen.target(), fui::makeRect(battery.x - 74, bar.y, 66, bar.height), work.label,
                chrome::lightHeader ? 0 : 3, fui::Color::Black, fui::TextAlign::Right);
  statusbattery::draw(screen.target(), battery, model.battery, model.charging, fui::Color::Black);
  if (model.wifiConnected) {
    const auto glyph = fui::makeRect(wifi.x + 2, wifi.y + (wifi.height - 40) / 2, 40, 40);
    screen.target().bitmap(glyph, fui::bitmapFromIcon(icon(Shortcut::Wifi)), fui::BitmapMode::Contain);
    screen.frame().hit(wifi, OpenSettings, int16_t(SettingsDestination::Wifi), fui::InputTouch);
  }
  if (bluetooth) {
    std::snprintf(work.label, sizeof(work.label), "%s: %s", tr(STR_BT_SHORT_NAME),
                  I18N.get(bluetoothui::bannerLabel(model.bluetooth)));
    const auto banner =
        fui::makeRect(g.header.x, bar.bottom() + kBluetoothSpacing.gap, g.header.width, kBluetoothSpacing.banner);
    screen.frame().hit(banner, OpenSettings, int16_t(SettingsDestination::Bluetooth), fui::InputTouch);
    screen.target().fill(banner, lightui::ink, lightui::radius());
    screen.target().stroke(banner, lightui::ink, chrome::lightHeader ? 1 : pixelui::kBorder, lightui::radius());
    fui::TextStyle status;
    status.font = fui::HEADER_CLOCK_FONT;
    status.color = fui::Color::White;
    status.align = fui::TextAlign::Center;
    fittext::draw(screen.target(), banner.inset({4, 12, 4, 12}), work.label, status, 1);
  }
  if (model.touch) {
    const int width = g.tiles.width / 4;
    const int size = chrome::lightHeader ? 44 : std::min(96, int(g.tiles.height) - 54);
    for (int n = 0; n < 4; ++n) {
      const auto shortcut = static_cast<Shortcut>(model.shortcuts[n]);
      const auto box = fui::makeRect(g.tiles.x + width * n + (width - size) / 2, g.tiles.y, size, size);
      work.button = {};
      work.button.styles = pixelui::buttonStyles();
      work.button.icon = fui::bitmapFromIcon(
          icon(shortcut, shortcut == Shortcut::Warm ? model.warmth > 0 : model.lightOn, model.rotateRight));
      work.button.iconSize = chrome::lightHeader ? 30 : 48;
      if (chrome::lightHeader) {
        work.button.styles = lightui::controlStyles(true);
        auto& styles = work.button.styles;
        styles.selected.background = styles.focused.background = styles.active.background = lightui::paper;
        const int border = shortcut == Shortcut::Light || shortcut == Shortcut::Warm ? 0 : 1;
        styles.selected.borderWidth = styles.focused.borderWidth = styles.active.borderWidth = border;
      }
      work.button.state =
          (shortcut == Shortcut::Dark && model.night) || (shortcut == Shortcut::Wifi && model.wifiConnected) ||
                  (shortcut == Shortcut::Bluetooth && model.bluetooth == bluetoothui::State::Connected) ||
                  (shortcut == Shortcut::Light && model.lightOn) || (shortcut == Shortcut::Warm && model.warmth > 0)
              ? fui::StateChecked
              : fui::StateNormal;
      work.button.action = Tile;
      work.button.value = model.shortcuts[n];
      work.button.minTouchSize = 0;
      screen.button(work.button, box);
      if (shortcut == Shortcut::Bluetooth)
        std::snprintf(work.label, sizeof(work.label), "%s\n%s", tr(STR_BT_SHORT_NAME),
                      I18N.get(bluetoothui::shortLabel(model.bluetooth)));
      else if (chrome::lightHeader)
        pixelui::copyNatural(work.label, sizeof(work.label), label(shortcut, model.lightOn, model.warmth > 0));
      else
        pixelui::copyCaps(work.label, sizeof(work.label), label(shortcut, model.lightOn, model.warmth > 0));
      fui::TextStyle caption;
      caption.font = chrome::lightHeader ? fui::HEADER_CLOCK_FONT : 3;
      caption.align = fui::TextAlign::Center;
      caption.maxLines = 2;
      const auto captionRect = fui::makeRect(g.tiles.x + n * width, box.bottom() + 6, width, 46);
      const auto previousClip = screen.target().clipRect();
      const bool clipped = screen.target().setClipRect(intersect(previousClip, captionRect));
      screen.target().text(captionRect, work.label, caption);
      if (clipped) screen.target().setClipRect(previousClip);
    }
    if (!chrome::lightHeader)
      screen.target().fill(fui::makeRect(g.tiles.x, g.tiles.bottom() + 10, g.tiles.width, 3),
                           fui::Paint::solid(fui::Color::Black));
  }
  if (model.light) {
    slider(screen, model, work, g.brightness, false);
    if (model.temperature) slider(screen, model, work, g.warmth, true);
  }
  work.button = {};
  work.button.styles.explicitlySet = true;
  work.button.action = Tile;
  work.button.value = -1;
  work.button.minTouchSize = 0;
  const int x = screen.device().width / 2, y = g.bottom - (chrome::lightHeader ? 22 : 28);
  screen.button(work.button,
                fui::makeRect(x - 48, g.bottom - (chrome::lightHeader ? 44 : 50), 96, chrome::lightHeader ? 44 : 46));
  const auto ink = fui::Paint::solid(fui::Color::Black);
  if (chrome::lightHeader)
    screen.target().fill(fui::makeRect(x - 28, y - 2, 56, 4), ink, 2);
  else {
    screen.target().line({int16_t(x - 20), int16_t(y + 5)}, {int16_t(x), int16_t(y - 5)}, 4, ink);
    screen.target().line({int16_t(x), int16_t(y - 5)}, {int16_t(x + 20), int16_t(y + 5)}, 4, ink);
  }
}
}  // namespace quickui
