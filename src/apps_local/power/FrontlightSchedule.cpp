#include "FrontlightSchedule.h"

#include <HalClock.h>
#include <HalFrontlight.h>

#include <algorithm>
#include <ctime>

#include "CrossPointSettings.h"

namespace frontlightschedule {
namespace {
bool applied = false;
uint8_t manualChannels = 0;
Levels before, scheduled;

Levels actual(const bool warm) {
  return {Frontlight.brightness(), warm ? Frontlight.warmth() : uint8_t{0}, Frontlight.isOn()};
}

void apply(const Levels light, const bool warm) {
  if (!light.on) Frontlight.setOn(false);
  Frontlight.setBrightness(light.brightness);
  if (warm) Frontlight.setWarmth(light.warmth);
  if (light.on) Frontlight.setOn(true);
}
}  // namespace

Levels manualLevels() {
  auto levels = actual(Frontlight.hasColorTemperature());
  if (applied) {
    if (!(manualChannels & Brightness) && levels.brightness == scheduled.brightness)
      levels.brightness = SETTINGS.frontlightBrightness;
    if (!(manualChannels & Warmth) && levels.warmth == scheduled.warmth) levels.warmth = SETTINGS.frontlightWarmth;
    if (!(manualChannels & Power) && levels.on == scheduled.on) levels.on = SETTINGS.frontlightOn != 0;
  }
  return levels;
}

uint8_t manualWarmthMemory(const uint8_t warmth) {
  return applied && !(manualChannels & Warmth) && warmth == scheduled.warmth ? SETTINGS.frontlightWarmthMemory : warmth;
}

void noteManual(const uint8_t channels) {
  if (!applied) return;
  manualChannels |= channels;
  if (channels & Brightness) before.brightness = Frontlight.brightness();
  if (channels & Warmth) before.warmth = Frontlight.warmth();
  if (channels & Power) before.on = Frontlight.isOn();
}

bool powerForRestart() {
  const bool on = Frontlight.isOn();
  return applied && !(manualChannels & Power) && on == scheduled.on ? before.on : on;
}

void poll() {
  static bool sampled = false;
  static uint32_t lastPoll = 0;
  static Levels configured;
  if (!Frontlight.present() || (!SETTINGS.frontlightSchedule && !applied)) return;
  const uint32_t now = millis();
  if (sampled && now - lastPoll < 1000) return;
  sampled = true;
  lastPoll = now;

  const bool warm = Frontlight.hasColorTemperature();
  const Levels current = actual(warm);
  unsigned minute = 0;
  const bool enabled = SETTINGS.frontlightSchedule != 0;
  if (enabled) {
    tm local{};
    if (!halClock.localTime(local) || local.tm_year < 120) {
      const auto epoch = std::time(nullptr);
      if (epoch <= 0 || !localtime_r(&epoch, &local) || local.tm_year < 120) return;
    }
    minute = local.tm_hour * 60u + local.tm_min;
  }
  const unsigned start = SETTINGS.frontlightStartHour * 60u + SETTINGS.frontlightStartMinute;
  const unsigned end = SETTINGS.frontlightEndHour * 60u + SETTINGS.frontlightEndMinute;
  if (!enabled || !activeAt(minute, start, end)) {
    if (applied) {
      const Levels restored{current.brightness == scheduled.brightness ? before.brightness : current.brightness,
                            current.warmth == scheduled.warmth ? before.warmth : current.warmth,
                            current.on == scheduled.on ? before.on : current.on};
      if (restored != current) apply(restored, warm);
    }
    applied = false;
    return;
  }

  const Levels desired{std::max<uint8_t>(1, SETTINGS.frontlightScheduledBrightness),
                       warm ? SETTINGS.frontlightScheduledWarmth : uint8_t{0}, true};
  if (!applied) before = current;
  if (!applied || configured != desired) {
    apply(desired, warm);
    configured = desired;
    scheduled = actual(warm);
    manualChannels = 0;
    applied = true;
  }
}
}  // namespace frontlightschedule
