#include "TimezonePickerActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "util/Timezones.h"

namespace fui = freeink::ui;

TimezonePickerActivity::TimezonePickerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("TimezonePicker", renderer, mappedInput, false, true) {}

void TimezonePickerActivity::onEnter() {
  UiListActivity::onEnter();
  const size_t count = timezones::count();
  offsetLabels_.resize(count);
  rowItems_.resize(count);
  for (size_t i = 0; i < count; i++) {
    const auto zone = timezones::pickerZoneAt(static_cast<uint8_t>(i));
    char offset[12];
    timezones::formatOffset(zone, offset, sizeof(offset));
    offsetLabels_[i] = offset;
    rowItems_[i].label = timezones::table()[zone].name;
    rowItems_[i].subtitle = offsetLabels_[i].c_str();
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
  // Open on the active zone rather than the top of a ~50-row list.
  nav.reset(timezones::pickerRowForZone(timezones::activeIndex()));
}

int TimezonePickerActivity::listCount() const { return static_cast<int>(timezones::count()); }

const char* TimezonePickerActivity::headerTitle() const { return tr(STR_TIMEZONE); }

void TimezonePickerActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  SETTINGS.clockTimezone = timezones::pickerZoneAt(static_cast<uint8_t>(index));
  SETTINGS.saveToFile();
  timezones::applyToClock();
  finish();
}

void TimezonePickerActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, true);
  useCompactSettingsRows();
  const auto& metrics = UITheme::getInstance().getMetrics();
  pixelui::begin(screen, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 1;
  syncListViewport(screen, props);
  pixelui::list(screen, props);
}
