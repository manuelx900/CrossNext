#include "SettingsActivity.h"

#include <BoardConfig.h>
#include <ChromeRegion.h>
#include <GfxRenderer.h>
#include <HalClock.h>
#include <HalDisplay.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "AboutActivity.h"
#include "ButtonRemapActivity.h"
#include "ClearCacheActivity.h"
#include "ClockSettingsActivity.h"
#include "CrossPointSettings.h"
#include "DeveloperModeActivity.h"
#include "FontDownloadActivity.h"
#include "HomeButtonSettingsActivity.h"
#include "KOReaderSettingsActivity.h"
#include "KeyboardLayoutsActivity.h"
#include "LanguageSelectActivity.h"
#include "MappedInputManager.h"
#include "OpdsServerListActivity.h"
#include "OtaUpdateActivity.h"
#include "SdCardFontSystem.h"
#include "SdFirmwareUpdateActivity.h"
#include "SettingsList.h"
#include "StatusBarSettingsActivity.h"
#include "TextSettingsActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/plugins/PluginCatalogActivity.h"
#include "activities/util/IntervalSelectionActivity.h"
#include "apps_local/home/HomeMenuSettingsActivity.h"
#include "apps_local/tools/FontManagementActivity.h"
#include "apps_local/tools/PresentationSettingsActivity.h"
#include "apps_local/tools/QuickSettingsConfigActivity.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "apps_local/ui/SettingsArt.h"
#include "apps_local/ui/SettingsHome.h"
#include "apps_local/ui/SettingsMenuStyle.h"
#include "apps_local/ui/ToyboxIcons.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"
#include "components/icons/listIcons.h"
#include "fontIds.h"

namespace fui = freeink::ui;
namespace {
int settingsGroup(const SettingInfo& s, int category) {
  if (category == 0) {
    if (s.valuePtr == &CrossPointSettings::sleepScreen || s.valuePtr == &CrossPointSettings::sleepScreenCoverMode ||
        s.valuePtr == &CrossPointSettings::sleepScreenCoverFilter ||
        s.valuePtr == &CrossPointSettings::quickResumeSleepScreen)
      return 0;
    if (s.action == SettingAction::UiPresentation) return 2;
    return 1;
  }
  if (category == 1) {
    if (s.action == SettingAction::TextSettings || s.action == SettingAction::DownloadFonts ||
        s.action == SettingAction::ReadingFormats)
      return 0;
    if (s.nameId == StrId::STR_WORD_DEFINITION_DICT || s.nameId == StrId::STR_WORD_TRANSLATION_DICT) return 2;
    if (s.action == SettingAction::CustomiseStatusBar) return 3;
    return 1;
  }
  if (category == 2) {
    if (s.action == SettingAction::KeyboardLayouts) return 3;
    if (s.valuePtr == &CrossPointSettings::touchScreenLocked ||
        s.valuePtr == &CrossPointSettings::touchReaderControls || s.valuePtr == &CrossPointSettings::pageTurnGesture ||
        s.valuePtr == &CrossPointSettings::previousPageGesture ||
        s.valuePtr == &CrossPointSettings::xtcInvertPageDirection)
      return 1;
    if (s.valuePtr == &CrossPointSettings::vibration || s.valuePtr == &CrossPointSettings::hapticIntensity) return 2;
    return 0;
  }
  if (s.action == SettingAction::HomeMenu || s.action == SettingAction::Language) return 0;
  if (s.valuePtr == &CrossPointSettings::sleepTimeoutMinutes) return 1;
  if (s.valuePtr == &CrossPointSettings::showHiddenFiles || s.valuePtr == &CrossPointSettings::libraryUseMetadata ||
      s.valuePtr == &CrossPointSettings::removeReadBooksFromRecents ||
      s.valuePtr == &CrossPointSettings::moveFinishedToReadFolder)
    return 2;
  if (s.action == SettingAction::Network || s.action == SettingAction::KOReaderSync ||
      s.action == SettingAction::OPDSBrowser)
    return 3;
  if (s.action == SettingAction::ClockSettings) return 4;
  return 5;
}
StrId settingsGroupLabel(int group, int category) {
  static constexpr StrId display[] = {StrId::STR_SETTINGS_SUSPENSION, StrId::STR_CAT_DISPLAY,
                                      StrId::STR_SETTINGS_APPEARANCE};
  static constexpr StrId reader[] = {StrId::STR_SETTINGS_TEXTS, StrId::STR_SETTINGS_READING,
                                     StrId::STR_SETTINGS_DICTIONARIES, StrId::STR_STATUS_BAR};
  static constexpr StrId controls[] = {StrId::STR_SETTINGS_BUTTONS, StrId::STR_SETTINGS_TOUCH,
                                       StrId::STR_SETTINGS_HAPTICS, StrId::STR_SETTINGS_KEYBOARD};
  static constexpr StrId system[] = {StrId::STR_SETTINGS_PERSONALISATION, StrId::STR_SETTINGS_POWER,
                                     StrId::STR_SETTINGS_FILES,           StrId::STR_SETTINGS_CONNECTIVITY,
                                     StrId::STR_SETTINGS_DATE_TIME,       StrId::STR_SETTINGS_DEVICE};
  return category == 0   ? display[group]
         : category == 1 ? reader[group]
         : category == 2 ? controls[group]
                         : system[group];
}
StrId settingHelp(const SettingInfo& setting) {
  if (setting.valuePtr == &CrossPointSettings::sideLeftAction ||
      setting.valuePtr == &CrossPointSettings::sideRightAction)
    return StrId::STR_SIDE_ACTION_HELP;
  if (setting.valuePtr == &CrossPointSettings::xtcInvertPageDirection) return StrId::STR_XTC_INVERT_DIRECTION_HELP;
  if (setting.valuePtr == &CrossPointSettings::removeReadBooksFromRecents ||
      setting.valuePtr == &CrossPointSettings::moveFinishedToReadFolder)
    return StrId::STR_SETTINGS_FINISHED_HELP;
  switch (setting.action) {
    case SettingAction::TextSettings:
      return StrId::STR_SETTINGS_TEXT_HELP;
    case SettingAction::DownloadFonts:
      return StrId::STR_SETTINGS_FONTS_HELP;
    case SettingAction::ReadingFormats:
      return StrId::STR_SETTINGS_FORMATS_HELP;
    case SettingAction::CustomiseStatusBar:
      return StrId::STR_SETTINGS_STATUS_HELP;
    case SettingAction::Network:
      return StrId::STR_SETTINGS_WIFI_HELP;
    case SettingAction::KOReaderSync:
      return StrId::STR_SETTINGS_SYNC_HELP;
    case SettingAction::OPDSBrowser:
      return StrId::STR_SETTINGS_OPDS_HELP;
    case SettingAction::ClockSettings:
      return StrId::STR_SETTINGS_CLOCK_HELP;
    case SettingAction::SdFirmwareUpdate:
      return StrId::STR_SETTINGS_SD_HELP;
    case SettingAction::Plugins:
      return StrId::STR_SETTINGS_PLUGINS_HELP;
    case SettingAction::About:
      return StrId::STR_SETTINGS_ABOUT_HELP;
    default:
      return StrId::STR_NONE_OPT;
  }
}
}  // namespace

SettingsActivity::SettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiTabListActivity("Settings", renderer, mappedInput) {
  setInterfaceFontSection(menufont::Section::Settings);
}

void SettingsActivity::rebuildSettingsLists() {
  displaySettings.clear();
  readerSettings.clear();
  controlsSettings.clear();
  systemSettings.clear();
  librarySettings.clear();
  connectivitySettings.clear();
  deviceSettings.clear();
  displaySettings.reserve(32);
  readerSettings.reserve(32);
  controlsSettings.reserve(32);
  systemSettings.reserve(32);
  librarySettings.reserve(8);
  connectivitySettings.reserve(8);
  deviceSettings.reserve(16);

  // Pick up any fonts uploaded/deleted over the web server since the last
  // reader activity ran — otherwise the font-family picker shows stale list.
  sdFontSystem.refreshIfDirty();

  // Rescan /dictionaries on every rebuild: cheap (one directory listing) and
  // picks up dictionaries copied to the SD card since the last visit.
  std::vector<DictionaryEntry> dictionaries;
  DictionaryRegistry::discover(dictionaries);

  // Filled by the loop, drained after the action rows below.
  std::vector<SettingInfo> forkSystemSettings;
  forkSystemSettings.reserve(2);

  for (auto& setting : getSettingsList(&sdFontSystem.registry(), &dictionaries)) {
    if (setting.nameId == StrId::STR_DEVICE_REPORT) continue;
    // home_button::isSetting: upstream's Home Button Gestures rows render on
    // their own screen, not in this list.
    if (setting.category == StrId::STR_NONE_OPT || home_button::isSetting(setting.valuePtr)) continue;
    if (BoardConfig::hasTouch() &&
        (setting.nameId == StrId::STR_SHORT_PWR_BTN || setting.valuePtr == &CrossPointSettings::doubleClickPwrLight ||
         setting.valuePtr == &CrossPointSettings::showReaderMenu ||
         setting.valuePtr == &CrossPointSettings::pwrBtnFootnoteBack))
      continue;
    if (setting.category == StrId::STR_CAT_INTERFACE) {
      continue;
    } else if (setting.category == StrId::STR_CAT_DISPLAY) {
      if (setting.valuePtr == &CrossPointSettings::uiTheme) {
        continue;
      }
      // The sunlight fading fix is a grayscale-waveform compensation that does
      // not apply on the X4 Pro / X4 Classic (plain OTP waveform, same panels).
      if (setting.valuePtr == &CrossPointSettings::fadingFix &&
          (BoardConfig::isX4Pro() || BoardConfig::isX4Classic())) {
        continue;
      }
      displaySettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_READER) {
      // Settings merged into "Text Settings"
      // (they stay in the shared list for the web settings API)
      if (setting.inTextSettings) continue;
      readerSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_CONTROLS) {
      if (BoardConfig::hasHomeKey() && setting.nameId == StrId::STR_LONG_PRESS_MENU) continue;
      if (setting.valuePtr == &CrossPointSettings::pwrBtnFootnoteBack &&
          SETTINGS.shortPwrBtn != CrossPointSettings::SHORT_PWRBTN::FOOTNOTES) {
        continue;
      }
      controlsSettings.push_back(setting);
    } else if (setting.category == StrId::STR_CAT_SYSTEM) {
      // CrossPlay's own settings sort below every CrossPoint one. Held back
      // rather than pushed here, because the action rows below are appended
      // after this loop and would otherwise sit underneath them.
      //
      // Both of these are DEVICE-wide: what the device reports, and whether it
      // accepts firmware over Wi-Fi. An app's own option never belongs here --
      // triviaShowUsCentric was in this list until card #311 and is now
      // category-less, so it never reaches this switch at all.
      if (setting.valuePtr == &CrossPointSettings::deviceReport || setting.valuePtr == &CrossPointSettings::devMode) {
        forkSystemSettings.push_back(setting);
        continue;
      }
      systemSettings.push_back(setting);
    }
  }

  // Append device-only ACTION items
  if (!BoardConfig::hasTouch()) {
    controlsSettings.insert(controlsSettings.begin(),
                            SettingInfo::Action(StrId::STR_REMAP_FRONT_BUTTONS, SettingAction::RemapFrontButtons));
  }
  if (BoardConfig::hasHomeKey()) {
    controlsSettings.insert(controlsSettings.begin(),
                            SettingInfo::Action(StrId::STR_HOME_BUTTON, SettingAction::HomeButton));
  }
  systemSettings.push_back(SettingInfo::Action(StrId::STR_WIFI_NETWORKS, SettingAction::Network));
  // Clock configuration only exists where the RTC probe found hardware; on
  // clockless boards there is nothing to set.
  if (halClock.isAvailable()) {
    systemSettings.push_back(SettingInfo::Action(StrId::STR_CLOCK, SettingAction::ClockSettings));
  }
  systemSettings.push_back(SettingInfo::Action(StrId::STR_KOREADER_SYNC, SettingAction::KOReaderSync));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_OPDS_SERVERS, SettingAction::OPDSBrowser));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_CLEAR_READING_CACHE, SettingAction::ClearCache));
  if (BoardConfig::isX4Pro())
    systemSettings.push_back(SettingInfo::Action(StrId::STR_CHECK_UPDATES, SettingAction::CheckForUpdates));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_SD_FIRMWARE_UPDATE, SettingAction::SdFirmwareUpdate));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_PLUGINS, SettingAction::Plugins));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_KEYBOARD_LAYOUTS, SettingAction::KeyboardLayouts));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_ABOUT, SettingAction::About));
  systemSettings.push_back(SettingInfo::Action(StrId::STR_LANGUAGE, SettingAction::Language));
  // The fork's own System rows go last, under every upstream row, so a reader
  // looking for a CrossPoint setting never scrolls past one of ours to reach
  // it. host-tests/settingsorder/ holds the line. See LOCAL_SCOPE.md.
  for (const auto& setting : forkSystemSettings) systemSettings.push_back(setting);
  systemSettings.insert(systemSettings.begin(), SettingInfo::Action(StrId::STR_HOME_MENU, SettingAction::HomeMenu));
  readerSettings.insert(readerSettings.begin(),
                        SettingInfo::Action(StrId::STR_READING_FORMATS, SettingAction::ReadingFormats));
  readerSettings.insert(readerSettings.begin(),
                        SettingInfo::Action(StrId::STR_TEXT_SETTINGS, SettingAction::TextSettings));
  readerSettings.insert(readerSettings.begin() + 1,
                        SettingInfo::Action(StrId::STR_MANAGE_FONTS, SettingAction::DownloadFonts));
  readerSettings.push_back(SettingInfo::Action(StrId::STR_CUSTOMISE_STATUS_BAR, SettingAction::CustomiseStatusBar));
  displaySettings.push_back(SettingInfo::Action(StrId::STR_QUICK_CUSTOMISE, SettingAction::QuickSettings));

  // Stable insertion ordering uses the existing vectors without a sorting allocation.
  std::vector<SettingInfo>* categories[] = {&displaySettings, &readerSettings, &controlsSettings, &systemSettings};
  for (int category = 0; category < 4; ++category) {
    auto& rows = *categories[category];
    for (size_t n = 1; n < rows.size(); ++n)
      for (size_t at = n; at > 0 && settingsGroup(rows[at], category) < settingsGroup(rows[at - 1], category); --at)
        std::swap(rows[at], rows[at - 1]);
  }
  // Split the old System list after its stable subgroup ordering. The shared
  // settings registry and JSON API categories remain compatible.
  for (auto it = systemSettings.begin(); it != systemSettings.end();) {
    const int group = settingsGroup(*it, 3);
    auto* destination = group == 2 || it->action == SettingAction::ClearCache ? &librarySettings
                        : group == 3                                          ? &connectivitySettings
                                                                              : nullptr;
    if (it->action == SettingAction::KeyboardLayouts) destination = &controlsSettings;
    if (group == 5 && !destination) destination = &deviceSettings;
    if (destination) {
      destination->push_back(std::move(*it));
      it = systemSettings.erase(it);
    } else
      ++it;
  }
  if (categoryHome)
    showCategoryHome();
  else
    selectCategory(selectedCategoryIndex);
}

void SettingsActivity::onEnter() {
  UiTabListActivity::onEnter();

  // Reset selection to first category (ring position 0, the tab bar, comes
  // from the base's per-tab nav reset)
  selectedCategoryIndex = 0;
  preserveQuickResumeTimeoutOn =
      SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT;
  quickResumeTimeoutAutoEnabled = false;
  syncQuickResumeTimeoutForSleepScreen(/*sleepScreenChanged=*/true, /*quickResumeTimeoutChanged=*/false);

  rebuildSettingsLists();
}

void SettingsActivity::selectCategory(const int categoryIndex) {
  selectedCategoryIndex = categoryIndex;
  categoryHome = false;
  const std::vector<SettingInfo>* categories[] = {&displaySettings, &readerSettings,       &controlsSettings,
                                                  &librarySettings, &connectivitySettings, &systemSettings,
                                                  &deviceSettings};
  currentSettings = categories[categoryIndex];
  settingsCount = static_cast<int>(currentSettings->size());
  activeNav().top = 0;  // category switches start the list at the top (no per-tab memory here)
  rebuildRowItems();
}

// Rebuilds rowValues_/rowItems_ (label + actionValue) for *currentSettings.
// Structural — call only when the active category or a category's setting
// list changes, never from buildScreen(), which only refreshes rowValues_
// content and rowItems_[].value pointers in place.
void SettingsActivity::rebuildRowItems() {
  const auto& settings = *currentSettings;
  rowValues_.assign(settings.size(), std::string());
  rowItems_.clear();
  rowItems_.reserve(settings.size());
  for (size_t i = 0; i < settings.size(); i++) {
    fui::ListItem item;
    item.label = I18N.get(settings[i].nameId);
    item.actionValue = static_cast<int16_t>(i);
    const auto section = [&](const SettingInfo& setting) -> StrId {
      if (selectedCategoryIndex < 3)
        return settingsGroupLabel(settingsGroup(setting, selectedCategoryIndex), selectedCategoryIndex);
      if (selectedCategoryIndex == 3) {
        if (setting.action == SettingAction::ClearCache) return StrId::STR_SETTINGS_MAINTENANCE;
        return setting.valuePtr == &CrossPointSettings::removeReadBooksFromRecents ||
                       setting.valuePtr == &CrossPointSettings::moveFinishedToReadFolder
                   ? StrId::STR_SETTINGS_FINISHED_BOOKS
                   : StrId::STR_SETTINGS_FILES;
      }
      if (selectedCategoryIndex == 5) return settingsGroupLabel(settingsGroup(setting, 3), 3);
      if (selectedCategoryIndex == 6 && setting.nameId == StrId::STR_DEV_MODE) return StrId::STR_SETTINGS_DEVELOPMENT;
      return StrId::STR_NONE_OPT;
    };
    const auto heading = section(settings[i]);
    if (heading != StrId::STR_NONE_OPT && (i == 0 || heading != section(settings[i - 1])))
      item.sectionHeading = I18N.get(heading);
    if (selectedCategoryIndex == 1 || selectedCategoryIndex == 4 || selectedCategoryIndex == 5 ||
        selectedCategoryIndex == 6) {
      const auto& s = settings[i];
      item.icon = s.action == SettingAction::Network                       ? settingsart::Wifi
                  : s.action == SettingAction::HomeMenu                    ? settingsart::Home
                  : s.action == SettingAction::Language                    ? settingsart::Globe
                  : s.action == SettingAction::ClockSettings               ? settingsart::Clock
                  : s.valuePtr == &CrossPointSettings::sleepTimeoutMinutes ? settingsart::Moon
                  : s.action == SettingAction::TextSettings                ? settingsart::Letters
                  : s.action == SettingAction::DownloadFonts               ? settingsart::Font
                  : s.action == SettingAction::ReadingFormats              ? settingsart::Stats
                  : s.valuePtr == &CrossPointSettings::orientation         ? settingsart::Orientation
                  : s.valuePtr == &CrossPointSettings::imageRendering      ? settingsart::Picture
                  : s.nameId == StrId::STR_WORD_TRANSLATION_DICT           ? settingsart::Translate
                  : s.action == SettingAction::CustomiseStatusBar          ? settingsart::Status
                  : s.action == SettingAction::CheckForUpdates             ? settingsart::Wifi
                  : s.action == SettingAction::SdFirmwareUpdate            ? settingsart::Sd
                  : s.action == SettingAction::Plugins                     ? settingsart::Puzzle
                  : s.action == SettingAction::About                       ? settingsart::Info
                  : s.nameId == StrId::STR_DEV_MODE                        ? settingsart::Code
                  : selectedCategoryIndex == 4                             ? settingsart::Grid
                                                                           : settingsart::Book;
    }
    rowItems_.push_back(item);
  }
}

void SettingsActivity::showCategoryHome() {
  static_assert(settingsui::homeCount == categoryCount + 1);
  categoryHome = true;
  settingsCount = settingsui::homeCount;
  currentSettings = &displaySettings;
  rowValues_.clear();
  rowItems_.clear();
  rowItems_.reserve(settingsui::homeCount);
  rowItems_.resize(settingsui::homeCount);
  for (int i = 0; i < settingsui::homeCount; ++i) {
    settingsui::homeRow(i, rowItems_[i]);
    pixelui::copyNatural(pixelTabLabels[i], sizeof(pixelTabLabels[i]), rowItems_[i].label);
    rowItems_[i].label = pixelTabLabels[i];
  }
  activeNav().reset();
  resetUi();
}

void SettingsActivity::onTabAction(const int index) {
  if (optionPopup.isActive()) return;
  selectCategory(index);
  activeNav().selected = 0;  // tab taps land with the tab bar focused
  // The switched-to tab repaints as the selected pill; a flash overlay on top
  // of it just repaints the pill in the focused style.
  app.clearTapFlash();
}

void SettingsActivity::activateIndex(const int index) {
  if (optionPopup.isActive()) return;
  if (categoryHome) {
    if (index == categoryCount) {
      auto next = makeUniqueNoThrow<PresentationSettingsActivity>(renderer, mappedInput,
                                                                  PresentationSettingsActivity::Kind::Theme);
      if (next)
        startActivityForResult(std::move(next), [this](const ActivityResult&) {
          RenderLock lock(*this);
          const auto navigation = activeNav();
          showCategoryHome();
          activeNav() = navigation;
          resetUi();
          requestUpdate();
        });
      else
        LOG_ERR("SETTINGS", "OOM: appearance settings");
      return;
    }
    app.clearTapFlash();
    selectCategory(index);
    activeNav().selected = 0;
    resetUi();
    requestUpdate();
    return;
  }
  (void)index;  // toggleCurrentSetting reads the ring position
  // Most rows repaint a different surface (popup, sub-activity, new value);
  // a lingering tap flash would gray an unrelated element.
  app.clearTapFlash();
  toggleCurrentSetting();
  // Tap-first: a tapped row is not a cursor position. Leaving it focused
  // (inverted) after the tap meant the row stayed black once its sub-screen or
  // popup closed, and Back then had to clear that focus before a second Back
  // left Settings. Hand the focus back to the tab band; the viewport stays put.
  if (mappedInput.hasTouch()) {
    activeNav().selected = 0;
  }
}

void SettingsActivity::onExit() {
  Activity::onExit();

  UITheme::getInstance().reload();  // Re-apply theme in case it was changed
}

void SettingsActivity::applyUiSettingChange(uint8_t CrossPointSettings::* valuePtr) {
  if (valuePtr == &CrossPointSettings::touchScreenLocked) {
    mappedInput.swallowCurrentTouch();
    return;
  }
  // Theme changes take effect immediately, on this screen — reload the theme
  // and re-derive the app's tokens so the very next repaint is in the new look.
  if (valuePtr != &CrossPointSettings::uiTheme) {
    return;
  }
  UITheme::getInstance().reload();
  // Re-derive the shared tokens for the new look; the gate stays closed until
  // the repaint that rebuilds the interaction table in the new layout.
  resetUi();
}

bool SettingsActivity::handleCustomInput() {
  return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); });
}

void SettingsActivity::stepTab(const int direction) {
  if (categoryHome) {
    moveRingTo(std::clamp(ringPos() + direction, 1, settingsui::homeCount));
    return;
  }
  // Ring position 0 stays on the tab bar; a row selection collapses to the
  // new category's first row (per-tab memory is deliberately not kept here).
  const bool onTabBar = ringPos() == 0;
  selectedCategoryIndex = direction > 0 ? ButtonNavigator::nextIndex(selectedCategoryIndex, categoryCount)
                                        : ButtonNavigator::previousIndex(selectedCategoryIndex, categoryCount);
  selectCategory(selectedCategoryIndex);
  activeNav().selected = onTabBar ? 0 : 1;
  requestUpdate();
}

bool SettingsActivity::handleButtons() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (categoryHome) {
      activateIndex(std::max(0, ringPos() - 1));
    } else if (ringPos() == 0) {
      showCategoryHome();
      requestUpdate();
    } else {
      toggleCurrentSetting();
      requestUpdate();
    }
    return true;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (!categoryHome) {
      showCategoryHome();
      requestUpdate();
    } else {
      SETTINGS.saveToFile();
      onGoHome();
    }
    return true;
  }

  return false;
}

void SettingsActivity::toggleCurrentSetting() {
  mappedInput.resetHomeButtonInput();
  int selectedSetting = ringPos() - 1;
  if (selectedSetting < 0 || selectedSetting >= settingsCount) {
    return;
  }

  const auto& setting = (*currentSettings)[selectedSetting];
  const auto changedValue = setting.valuePtr;
  const bool sleepScreenChanged = setting.valuePtr == &CrossPointSettings::sleepScreen;
  const bool quickResumeTimeoutChanged = setting.valuePtr == &CrossPointSettings::quickResumeSleepScreen;

  if (setting.valuePtr == &CrossPointSettings::sideLeftAction ||
      setting.valuePtr == &CrossPointSettings::sideRightAction) {
    const auto binding = setting.valuePtr == &CrossPointSettings::sideLeftAction
                             ? HomeButtonSettingsActivity::Binding::SideLeft
                             : HomeButtonSettingsActivity::Binding::SideRight;
    auto next = makeUniqueNoThrow<HomeButtonSettingsActivity>(renderer, mappedInput, binding);
    if (!next) {
      LOG_ERR("SET", "OOM: side button settings");
      return;
    }
    startActivityForResult(std::move(next), [this](const ActivityResult&) {
      rebuildSettingsLists();
      resetUi();
      requestUpdate();
    });
    return;
  }

  if (setting.nameId == StrId::STR_TIME_TO_SLEEP) {
    openSleepTimeoutPicker();
    return;
  }

  // Developer Mode is a TOGGLE so it persists and shows On/Off like any other,
  // but selecting it opens a screen rather than flipping in place. It has to:
  // turning it on produces a pairing code, and a code nobody can read is the
  // same as no code at all. The screen owns the flip.
  if (setting.nameId == StrId::STR_DEV_MODE) {
    startActivityForResult(std::make_unique<DeveloperModeActivity>(renderer, mappedInput), nullptr);
    return;
  }

  if (setting.type == SettingType::TOGGLE && setting.valuePtr != nullptr) {
    // Toggle the boolean value using the member pointer
    const bool currentValue = SETTINGS.*(setting.valuePtr);
    SETTINGS.*(setting.valuePtr) = !currentValue;
  } else if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    const uint8_t currentValue = SETTINGS.*(setting.valuePtr);
    const auto enumLabels = setting.enumLabels();
    if (enumLabels.size() > 2) {
      const auto valuePtr = setting.valuePtr;
      optionPopup.show(setting.nameId, enumLabels.data(), static_cast<int>(enumLabels.size()), currentValue,
                       [this, valuePtr, sleepScreenChanged, quickResumeTimeoutChanged](int idx) {
                         SETTINGS.*valuePtr = idx;
                         syncQuickResumeTimeoutForSleepScreen(sleepScreenChanged, quickResumeTimeoutChanged);
                         SETTINGS.saveToFile();
                         rebuildSettingsLists();
                         applyUiSettingChange(valuePtr);
                       });
      requestUpdate();
      return;
    }
    SETTINGS.*(setting.valuePtr) = (currentValue + 1) % static_cast<uint8_t>(enumLabels.size());
  } else if (setting.type == SettingType::ENUM && setting.valueGetter && setting.valueSetter) {
    const uint8_t totalValues = setting.enumStringValues.empty()
                                    ? static_cast<uint8_t>(setting.enumLabels().size())
                                    : static_cast<uint8_t>(setting.enumStringValues.size());
    const uint8_t cur = setting.valueGetter();
    const bool dictionary =
        setting.nameId == StrId::STR_WORD_DEFINITION_DICT || setting.nameId == StrId::STR_WORD_TRANSLATION_DICT;
    if (dictionary && totalValues == 1) {
      const char* options[] = {tr(STR_OK_BUTTON)};
      optionPopup.showMessage(I18N.get(setting.nameId), tr(STR_DICTIONARY_INSTALL_HELP), options, 1, 0,
                              [this](int) { requestUpdate(); });
      requestUpdate();
      return;
    }
    if (totalValues > 2 || dictionary) {
      const auto valueSetter = setting.valueSetter;
      auto onSelect = [this, valueSetter, sleepScreenChanged, quickResumeTimeoutChanged](int idx) {
        valueSetter(idx);
        syncQuickResumeTimeoutForSleepScreen(sleepScreenChanged, quickResumeTimeoutChanged);
        SETTINGS.saveToFile();
        rebuildSettingsLists();
      };
      if (!setting.enumStringValues.empty()) {
        optionPopup.show(setting.nameId, setting.enumStringValues, cur, std::move(onSelect));
      } else {
        const auto enumLabels = setting.enumLabels();
        optionPopup.show(setting.nameId, enumLabels.data(), static_cast<int>(enumLabels.size()), cur,
                         std::move(onSelect));
      }
      requestUpdate();
      return;
    }
    setting.valueSetter((cur + 1) % totalValues);
  } else if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    const int8_t currentValue = SETTINGS.*(setting.valuePtr);
    if (currentValue + setting.valueRange.step > setting.valueRange.max) {
      SETTINGS.*(setting.valuePtr) = setting.valueRange.min;
    } else {
      SETTINGS.*(setting.valuePtr) = currentValue + setting.valueRange.step;
    }
  } else if (setting.type == SettingType::ACTION) {
    auto resultHandler = [this](const ActivityResult&) { SETTINGS.saveToFile(); };

    switch (setting.action) {
      case SettingAction::QuickSettings: {
        auto next = makeUniqueNoThrow<QuickSettingsConfigActivity>(renderer, mappedInput);
        if (next)
          startActivityForResult(std::move(next), [this](const ActivityResult&) { requestUpdate(); });
        else
          LOG_ERR("SETTINGS", "OOM: quick settings");
        return;
      }
      case SettingAction::HomeMenu: {
        auto activity = makeUniqueNoThrow<HomeMenuSettingsActivity>(renderer, mappedInput);
        if (activity)
          startActivityForResult(std::move(activity), [this](const ActivityResult&) { requestUpdate(); });
        else
          LOG_ERR("SETTINGS", "OOM: home menu settings");
        return;
      }
      case SettingAction::HomeButton: {
        // Activities must outlive this call and are owned by the activity stack.
        auto activity = makeUniqueNoThrow<HomeButtonSettingsActivity>(renderer, mappedInput);
        if (!activity) {
          LOG_ERR("SET", "OOM: Home button settings");
          return;
        }
        startActivityForResult(std::move(activity), [this](const ActivityResult&) { requestUpdate(); });
        return;
      }
      case SettingAction::RemapFrontButtons:
        startActivityForResult(std::make_unique<ButtonRemapActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::CustomiseStatusBar:
        startActivityForResult(std::make_unique<StatusBarSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::ClockSettings:
        if (auto activity = makeUniqueNoThrow<ClockSettingsActivity>(renderer, mappedInput)) {
          startActivityForResult(std::move(activity), resultHandler);
        } else {
          LOG_ERR("SETTINGS", "OOM: ClockSettingsActivity");
        }
        break;
      case SettingAction::KOReaderSync:
        startActivityForResult(std::make_unique<KOReaderSettingsActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::OPDSBrowser:
        startActivityForResult(std::make_unique<OpdsServerListActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::Network: {
        auto activity =
            makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput, false, WifiSelectionPurpose::Manage);
        if (!activity) {
          LOG_ERR("SETTINGS", "OOM: WifiSelectionActivity");
          return;
        }
        startActivityForResult(std::move(activity), resultHandler);
        break;
      }
      case SettingAction::ClearCache:
        startActivityForResult(std::make_unique<ClearCacheActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::CheckForUpdates: {
        auto updater = makeUniqueNoThrow<OtaUpdateActivity>(renderer, mappedInput);
        if (!updater) {
          LOG_ERR("SET", "OOM: OTA update screen");
          break;
        }
        startActivityForResult(std::move(updater), resultHandler);
        break;
      }
      case SettingAction::SdFirmwareUpdate:
        startActivityForResult(std::make_unique<SdFirmwareUpdateActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::DownloadFonts: {
        auto fonts = makeUniqueNoThrow<FontManagementActivity>(renderer, mappedInput);
        if (!fonts) {
          LOG_ERR("SET", "OOM: font management");
          break;
        }
        startActivityForResult(std::move(fonts), [this](const ActivityResult&) {
          SETTINGS.saveToFile();
          rebuildSettingsLists();
        });
        break;
      }
      case SettingAction::TextSettings:
        startActivityForResult(std::make_unique<TextSettingsActivity>(renderer, mappedInput, &sdFontSystem.registry(),
                                                                      TextSettingsActivity::Tab::Family),
                               [this](const ActivityResult&) {
                                 // TextSettingsActivity saves on each change; no save needed here.
                                 rebuildSettingsLists();
                               });
        break;
      case SettingAction::Language:
        // Row labels are translated once in rebuildRowItems() and don't
        // re-run on Pop (see ActivityManager::loop()), so a language switch
        // needs an explicit rebuild here rather than the generic resultHandler.
        startActivityForResult(std::make_unique<LanguageSelectActivity>(renderer, mappedInput),
                               [this](const ActivityResult&) {
                                 SETTINGS.saveToFile();
                                 rebuildSettingsLists();
                               });
        break;
      case SettingAction::Plugins:
        startActivityForResult(std::make_unique<PluginCatalogActivity>(renderer, mappedInput), resultHandler);
        break;
      case SettingAction::KeyboardLayouts:
        if (auto activity = makeUniqueNoThrow<KeyboardLayoutsActivity>(renderer, mappedInput)) {
          startActivityForResult(std::move(activity), nullptr);
        } else {
          LOG_ERR("SETTINGS", "OOM: KeyboardLayoutsActivity");
        }
        break;
      case SettingAction::About:
        if (auto activity = makeUniqueNoThrow<AboutActivity>(renderer, mappedInput)) {
          startActivityForResult(std::move(activity), nullptr);
        } else {
          LOG_ERR("SETTINGS", "OOM: AboutActivity");
        }
        break;
      case SettingAction::UiPresentation:
      case SettingAction::ReadingFormats: {
        const auto kind = setting.action == SettingAction::UiPresentation
                              ? PresentationSettingsActivity::Kind::Theme
                              : PresentationSettingsActivity::Kind::ReadingFormats;
        auto next = makeUniqueNoThrow<PresentationSettingsActivity>(renderer, mappedInput, kind);
        if (next)
          startActivityForResult(std::move(next), resultHandler);
        else
          LOG_ERR("SETTINGS", "OOM: presentation settings");
        break;
      }
      case SettingAction::None:
        // Do nothing
        break;
    }
    return;  // Results will be handled in the result handler, so we can return early here
  } else {
    return;
  }

  syncQuickResumeTimeoutForSleepScreen(sleepScreenChanged, quickResumeTimeoutChanged);
  SETTINGS.saveToFile();
  rebuildSettingsLists();
  applyUiSettingChange(changedValue);
  activeNav().selected = std::min(ringPos(), settingsCount);
}

void SettingsActivity::syncQuickResumeTimeoutForSleepScreen(bool sleepScreenChanged, bool quickResumeTimeoutChanged) {
  if (quickResumeTimeoutChanged) {
    preserveQuickResumeTimeoutOn =
        SETTINGS.quickResumeSleepScreen == CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT;
    quickResumeTimeoutAutoEnabled = false;
  }

  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::QUICK_RESUME) {
    if (SETTINGS.quickResumeSleepScreen != CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT) {
      SETTINGS.quickResumeSleepScreen = CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT;
      quickResumeTimeoutAutoEnabled = !preserveQuickResumeTimeoutOn;
    } else if (sleepScreenChanged && !preserveQuickResumeTimeoutOn) {
      quickResumeTimeoutAutoEnabled = true;
    }
    return;
  }

  if (sleepScreenChanged && quickResumeTimeoutAutoEnabled && !preserveQuickResumeTimeoutOn) {
    SETTINGS.quickResumeSleepScreen = CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_NEVER;
    quickResumeTimeoutAutoEnabled = false;
  }
}

void SettingsActivity::openSleepTimeoutPicker() {
  startActivityForResult(
      std::make_unique<IntervalSelectionActivity>(
          renderer, mappedInput, "SleepTimeoutInterval", StrId::STR_TIME_TO_SLEEP, SETTINGS.sleepTimeoutMinutes,
          CrossPointSettings::MIN_SLEEP_TIMEOUT_MINUTES, CrossPointSettings::MAX_SLEEP_TIMEOUT_MINUTES, 1, 5,
          StrId::STR_SLEEP_TIMER_VALUE_FORMAT, false, StrId::STR_SLEEP_NEVER),
      [this](const ActivityResult& result) {
        if (!result.isCancelled) {
          SETTINGS.sleepTimeoutMinutes = static_cast<uint8_t>(std::get<IntervalResult>(result.data).value);
          SETTINGS.saveToFile();
        }
        requestUpdate();
      });
}

std::string SettingsActivity::settingValueText(const SettingInfo& setting) {
  if (setting.action == SettingAction::HomeButton) return tr(STR_CONFIGURE);
  if (setting.action == SettingAction::Language) return I18N.getLanguageName(I18N.getLanguage());
  if (setting.type == SettingType::ENUM && setting.valuePtr != nullptr) {
    // Guard like the valueGetter branch below: a corrupt/migrated settings
    // byte must not index past the enum table.
    const uint8_t value = SETTINGS.*(setting.valuePtr);
    const auto enumLabels = setting.enumLabels();
    if (value >= enumLabels.size()) return "";
    return I18N.get(enumLabels[value]);
  }
  if (setting.type == SettingType::ENUM && setting.valueGetter) {
    const uint8_t value = setting.valueGetter();
    if (!setting.enumStringValues.empty() && value < setting.enumStringValues.size()) {
      return setting.enumStringValues[value];
    }
    const auto enumLabels = setting.enumLabels();
    if (value < enumLabels.size()) {
      return I18N.get(enumLabels[value]);
    }
    return "";
  }
  if (setting.type == SettingType::VALUE && setting.valuePtr != nullptr) {
    if (setting.nameId == StrId::STR_TIME_TO_SLEEP) {
      if (SETTINGS.sleepTimeoutMinutes >= CrossPointSettings::SLEEP_TIMEOUT_NEVER_MINUTES) {
        return tr(STR_SLEEP_NEVER);
      }
      char valueBuffer[32];
      snprintf(valueBuffer, sizeof(valueBuffer), tr(STR_SLEEP_TIMER_VALUE_FORMAT),
               static_cast<unsigned int>(SETTINGS.*(setting.valuePtr)));
      return valueBuffer;
    }
    return std::to_string(SETTINGS.*(setting.valuePtr));
  }
  return "";
}

void SettingsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  const bool pixel = toybox::interfaceFontStyle() == menufont::Style::Pixel;
  uiTarget.setFont(0, pixel ? UI_12_FONT_ID : toybox::kButtonFontId);
  uiTarget.setFont(4, pixel ? UI_10_FONT_ID : toybox::kLibraryFontId);
  const int hints = mappedInput.hasTouch() ? 0 : UITheme::getInstance().getMetrics().buttonHintsHeight;
  pixelui::begin(screen, hints);
  // rowItems_ (label/actionValue) was built by rebuildRowItems() when the
  // category was last selected/rebuilt; only the live value text needs
  // refreshing here, by assigning into the existing rowValues_ strings (no
  // vector growth) rather than building a new items/values vector on every
  // render.
  const auto& settings = *currentSettings;
  for (size_t i = 0; !categoryHome && i < settings.size(); i++) {
    const auto& setting = settings[i];
    if (chrome::lightHeader) rowItems_[i].icon = {};
    const auto labels = setting.enumLabels();
    const auto helpId = settingHelp(setting);
    const char* help = helpId == StrId::STR_NONE_OPT ? nullptr : I18N.get(helpId);
    const bool checkbox = setting.type == SettingType::TOGGLE ||
                          (setting.type == SettingType::ENUM && setting.enumStringValues.empty() &&
                           labels.size() == 2 && labels[0] == StrId::STR_STATE_OFF && labels[1] == StrId::STR_STATE_ON);
    if (checkbox && (setting.valuePtr || setting.valueGetter)) {
      const bool checked = setting.valuePtr ? SETTINGS.*(setting.valuePtr) != 0 : setting.valueGetter() != 0;
      rowValues_[i].clear();
      rowItems_[i].subtitle = chrome::lightHeader ? nullptr : help;
      rowItems_[i].value = nullptr;
      GUI.setCheckboxRow(rowItems_[i], checked);
    } else {
      rowValues_[i] = settingValueText(setting);
      rowItems_[i].toggle = false;
      rowItems_[i].subtitle = rowValues_[i].empty() ? help : rowValues_[i].c_str();
      rowItems_[i].value = ">";
      {
        if (chrome::lightHeader) rowItems_[i].icon = {};
        const int available = screen.body().width - (chrome::lightHeader ? 64 : 108);
        fui::TextStyle titleStyle;
        titleStyle.font = 0;
        fui::TextStyle valueStyle;
        valueStyle.font = 0;
        const int used = screen.target().measureText(0, rowItems_[i].label, titleStyle).width +
                         screen.target().measureText(0, rowValues_[i].c_str(), valueStyle).width + 24;
        if (!rowValues_[i].empty() && used <= available) {
          rowItems_[i].value = rowValues_[i].c_str();
          rowItems_[i].subtitle = nullptr;
        } else if (rowValues_[i].empty())
          rowItems_[i].subtitle = nullptr;
      }
    }
  }

  fui::ListProps props;
  props.items = rowItems_.data();
  props.count = static_cast<uint16_t>(rowItems_.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  settingsui::styleRows(props);
  if (categoryHome) settingsui::styleHome(props);

  props.rtl =
      (I18N.getLanguage() == Language::AR || I18N.getLanguage() == Language::FA || I18N.getLanguage() == Language::HE);
  syncTabListViewport(screen, props);
  if (chrome::lightHeader)
    fui::list(screen.frame(), screen.body(), props);
  else
    pixelui::list(screen, props, false, true, pixelui::kBorder, true);
}

void SettingsActivity::drawChrome() {
  pixelui::drawHeader(renderer, categoryHome ? tr(STR_SETTINGS_TITLE) : I18N.get(categoryNames[selectedCategoryIndex]),
                      I18N.getLanguage() != Language::EN);
}

void SettingsActivity::drawFooter() {
  const int ring = ringPos();
  const char* confirmLabel = categoryHome || ring == 0 ? tr(STR_SELECT) : tr(STR_TOGGLE);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), confirmLabel, tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void SettingsActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
