#include "CrossPointSettings.h"

#include <BlePageTurnerJson.h>
#include <BoardConfig.h>
#include <ChromeRegion.h>
#include <I18n.h>
#include <LanguagePack.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>

#include "I18nKeys.h"
#include "ReaderFontSizes.h"
#include "ReaderLineSpacing.h"
#include "SettingsList.h"
#include "apps_local/readerbars/ReaderBarsJson.h"
#include "apps_local/readertext/BookTextPreferences.h"
#include "apps_local/reading/ReadingGoal.h"
#include "fontIds.h"
#include "util/InterfaceThemeStyle.h"
#include "util/ParagraphIndentMigration.h"
#include "util/PluginLocations.h"

namespace {

// Stack buffer for "<key>_obf" key construction — avoids a std::string
// allocation per obfuscated setting on every save and load.
constexpr size_t OBF_KEY_BUF = 64;

// Null-terminated copy into a fixed-size settings field.
void copyToField(char* dest, const char* src, const size_t maxLen) {
  strncpy(dest, src, maxLen - 1);
  dest[maxLen - 1] = '\0';
}

}  // namespace

void CrossPointSettings::validateFrontButtonMapping(CrossPointSettings& settings) {
  const uint8_t mapping[] = {settings.frontButtonBack, settings.frontButtonConfirm, settings.frontButtonLeft,
                             settings.frontButtonRight};
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = i + 1; j < 4; j++) {
      if (mapping[i] == mapping[j]) {
        settings.frontButtonBack = FRONT_HW_BACK;
        settings.frontButtonConfirm = FRONT_HW_CONFIRM;
        settings.frontButtonLeft = FRONT_HW_LEFT;
        settings.frontButtonRight = FRONT_HW_RIGHT;
        return;
      }
    }
  }
}

uint8_t CrossPointSettings::sleepTimeoutEnumToMinutes(const uint8_t legacyValue) {
  switch (legacyValue) {
    case SLEEP_1_MIN:
      return 1;
    case SLEEP_5_MIN:
      return 5;
    case SLEEP_15_MIN:
      return 15;
    case SLEEP_30_MIN:
      return 30;
    case SLEEP_10_MIN:
    default:
      return 10;
  }
}

void CrossPointSettings::toJson(JsonDocument& doc) const {
  bleturner::writeJson(ble, doc);
  readerbars::writeJson(doc, readerBars);
  const CrossPointSettings& s = *this;
  doc["readingDailyGoal"] = readingDailyGoal;
  doc["readingGoalStartDay"] = readingGoalStartDay;
  doc["readingGoalLegacyHistory"] = readingGoalLegacyHistory;
  doc["readingGoalHistoryVersion"] = readinggoal::historyVersion;
  doc["wordsTextAlignment"] = wordsTextAlignment;
  doc["flashcardTextAlignment"] = flashcardTextAlignment;

  auto visit = [&](const SettingInfo& info) {
    if (!info.key) return;
    // Dynamic entries (KOReader etc.) are stored in their own files — skip.
    if (!info.valuePtr && !info.stringOffset) return;

    if (info.stringOffset) {
      const char* strPtr = (const char*)&s + info.stringOffset;
      if (info.obfuscated) {
        char obfKey[OBF_KEY_BUF];
        snprintf(obfKey, sizeof(obfKey), "%s_obf", info.key);
        doc[obfKey] = obfuscation::obfuscateToBase64(strPtr);
      } else {
        doc[info.key] = strPtr;
      }
    } else {
      doc[info.key] = s.*(info.valuePtr);
    }
  };
  forEachBaseSetting(visit);
  // Front button remap — managed by RemapFrontButtons sub-activity, not in SettingsList.
  doc["frontButtonBack"] = frontButtonBack;
  doc["frontButtonConfirm"] = frontButtonConfirm;
  doc["frontButtonLeft"] = frontButtonLeft;
  doc["frontButtonRight"] = frontButtonRight;
  // Font family and size — both use dynamic getter/setters in SettingsList (the
  // option lists depend on the SD font registry), so the generic loop skips them.
  doc["fontFamily"] = fontFamily;
  doc["fontSize"] = fontPointSize;
  doc["readerFontStorageVersion"] = 1;
  doc["fontWeight"] = fontWeight;
  doc["libraryView"] = libraryView;
  doc["libraryLayout"] = libraryLayout;
  doc["libraryClassicGrid"] = libraryClassicGrid;
  doc["libraryCollectionGrid"] = libraryCollectionGrid;
  doc["libraryClassicExpanded"] = libraryClassicExpanded;
  doc["librarySortMethod"] = librarySortMethod;
  doc["librarySortDirections"] = librarySortDirections;
  doc["libraryExpandedList"] = libraryExpandedList;
  doc["libraryDisplay"] = libraryDisplay;
  doc["libraryFileTypes"] = libraryFileTypes;
  doc["weekStartsSunday"] = weekStartsSunday;
  doc["readingFormats"] = readingFormats;
  doc["homeLayout"] = homeLayout;
  doc["homeCoverLoadingMask"] = homeCoverLoadingMask;
  doc["homeLayoutVersion"] = 2;
  doc["interfaceThemeStyle"] = interfaceThemeStyle;
  doc["interfaceHeaderColor"] = interfaceHeaderColor;
  auto themeOverrides = doc["interfaceThemeOverrides"].to<JsonArray>();
  for (int n = 2; n < menufont::kSectionCount; ++n) themeOverrides.add(interfaceThemeOverrides[n]);
  auto fonts = doc["menuFontOverrides"].to<JsonArray>();
  for (int n = 1; n < menufont::kSectionCount; ++n) fonts.add(menuFontOverrides[n]);
  auto contentFonts = doc["menuFontItems"].to<JsonObject>();
  for (int n = 0; n < menufont::kItemCount; ++n)
    if (menuFontItemOverrides[n]) contentFonts[menufont::kItemKeys[n]] = menuFontItemOverrides[n];
  auto quick = doc["quickActions"].to<JsonArray>();
  for (uint8_t action : quickActions) quick.add(action);
  // SD card font family name — not in SettingsList, save manually
  if (sdFontFamilyName[0] != '\0') {
    doc["sdFontFamilyName"] = sdFontFamilyName;
  }
  if (*translationDictionaryName) doc["translationDictionaryName"] = translationDictionaryName;
  doc["dictionaryAuto"] = dictionaryAuto;
  doc["translationDictionaryAuto"] = translationDictionaryAuto;
  // Dictionary folder name — uses dynamic getter/setter in SettingsList, save manually
  if (dictionaryName[0] != '\0') {
    doc["dictionaryName"] = dictionaryName;
  }
  // The displayed option list varies by board, so this setting uses a dynamic
  // display-index mapping and is skipped by the generic persistence loop.
  doc["longPressMenuFunction"] = longPressMenuFunction;
  doc["shortPwrBtn"] = shortPwrBtn;
  doc["homeButtonTapAction"] = uint8_t(HomeButtonAction::Home);
  doc["pluginHubPromptHidden"] = pluginHubPromptHidden;
  doc["perBookTextSettings"] = perBookTextSettings;

  // Language -- managed by LanguageSelectActivity, not in SettingsList.
  // Stored as ISO code string ("EN", "DE", ...) for stability across enum reorders.
  doc["language"] = (language < getLanguageCount()) ? LANGUAGE_CODES[language] : "EN";
  if (languagePack[0]) doc["languagePack"] = languagePack;

  // A uint16_t mask, so it does not fit the uint8_t generic loop. Omitted while
  // unconfigured, so the default keeps following the UI language.
  if (keyboardLayouts != 0) {
    doc["keyboardLayouts"] = keyboardLayouts;
  }
  if (keyboardLanguageLayouts) {
    auto layouts = doc["keyboardLanguageLayouts"].to<JsonArray>();
    for (uint8_t n = 0; n < getLanguageCount(); ++n)
      if (keyboardLanguageLayouts & (uint64_t{1} << n)) layouts.add(LANGUAGE_CODES[n]);
  }
  booktext::writeGlobalDefaults(doc);
}

bool CrossPointSettings::fromJson(JsonVariantConst doc) {
  bleturner::readJson(ble, doc);
  CrossPointSettings& s = *this;
  bool needsResave = false;
  pluginHubPromptHidden = doc["pluginHubPromptHidden"] | uint8_t(0);
  perBookTextSettings =
      doc["perBookTextSettings"].is<uint8_t>() ? uint8_t(doc["perBookTextSettings"].as<uint8_t>() != 0) : 1;
  readingDailyGoal = doc["readingDailyGoal"].is<uint8_t>() && doc["readingDailyGoal"].as<uint8_t>() < 10
                         ? doc["readingDailyGoal"].as<uint8_t>()
                         : 0;
  const int startDay = doc["readingGoalStartDay"] | 0;
  readingGoalStartDay = readingDailyGoal && startDay >= 18262 && startDay <= 84005 ? startDay : 0;
  // Older saves did not distinguish an actual goal start from a migration date.
  // Preserve fulfilled journal days without inventing failures before that date.
  readingGoalLegacyHistory =
      readingDailyGoal && readinggoal::preserveLegacyHistory(doc["readingGoalHistoryVersion"] | 0u,
                                                             doc["readingGoalLegacyHistory"] | false);
  wordsTextAlignment = std::min<uint8_t>(doc["wordsTextAlignment"] | uint8_t(0), 3);
  flashcardTextAlignment = std::min<uint8_t>(doc["flashcardTextAlignment"] | uint8_t(0), 3);
  const auto commonTheme = doc["interfaceThemeStyle"];
  const auto headerColor = doc["interfaceHeaderColor"];
  interfaceHeaderColor =
      headerColor.is<uint8_t>() ? uint8_t(chrome::normalizedHeaderColor(headerColor.as<uint8_t>())) : 0;
  interfaceThemeStyle = commonTheme.is<uint8_t>() ? uint8_t(interfacetheme::normalized(commonTheme.as<uint8_t>())) : 0;
  const auto themeOverrides = doc["interfaceThemeOverrides"];
  for (int n = 2; n < menufont::kSectionCount; ++n) {
    const auto value = themeOverrides[n - 2];
    interfaceThemeOverrides[n] = value.is<uint8_t>() ? uint8_t(interfacetheme::normalized(value.as<uint8_t>())) : 0;
  }
  const auto fonts = doc["menuFontOverrides"];
  for (int n = 1; n < menufont::kSectionCount; ++n) {
    const auto value = fonts[n - 1];
    menuFontOverrides[n] = value.is<uint8_t>() ? uint8_t(menufont::normalizedOverride(value.as<uint8_t>())) : 0;
    if (value.is<uint8_t>() && value.as<uint8_t>() != menuFontOverrides[n]) needsResave = true;
  }
  for (int n = 0; n < menufont::kItemCount; ++n) {
    const auto value = doc["menuFontItems"][menufont::kItemKeys[n]];
    menuFontItemOverrides[n] = value.is<uint8_t>() ? uint8_t(menufont::normalizedOverride(value.as<uint8_t>())) : 0;
    if (value.is<uint8_t>() && value.as<uint8_t>() != menuFontItemOverrides[n]) needsResave = true;
  }
  if (doc["quickActions"].is<JsonArrayConst>() && doc["quickActions"].size() == quickui::kSlots) {
    uint16_t used = 0;
    for (int n = 0; n < quickui::kSlots; ++n) {
      const auto value = doc["quickActions"][n];
      uint8_t action = value.is<uint8_t>() ? value.as<uint8_t>() : uint8_t(quickui::Shortcut::Count);
      if (action >= uint8_t(quickui::Shortcut::Count) || (used & (1u << action))) {
        needsResave = true;
        action = 0;
        while (used & (1u << action)) ++action;
      }
      quickActions[n] = action;
      used |= 1u << action;
    }
  } else if (!doc["quickActions"].isNull())
    needsResave = true;

  auto clamp = [](uint8_t val, uint8_t maxVal, uint8_t def) -> uint8_t { return val < maxVal ? val : def; };

  auto visit = [&](const SettingInfo& info) {
    if (!info.key) return;
    // Dynamic entries (KOReader etc.) are stored in their own files — skip.
    if (!info.valuePtr && !info.stringOffset) return;

    if (info.stringOffset) {
      // destPtr starts out holding the struct-initializer default; it stays that
      // way unless the document actually carries a value for this key.
      char* destPtr = (char*)&s + info.stringOffset;
      if (info.stringMaxLen == 0) {
        LOG_ERR("CPS", "Misconfigured SettingInfo: stringMaxLen is 0 for key '%s'", info.key);
        destPtr[0] = '\0';
        needsResave = true;
        return;
      }

      bool loaded = false;
      if (info.obfuscated) {
        char obfKey[OBF_KEY_BUF];
        snprintf(obfKey, sizeof(obfKey), "%s_obf", info.key);
        bool ok = false;
        bool tooLong = false;
        const std::string decoded =
            obfuscation::deobfuscateFromBase64(doc[obfKey] | "", info.stringMaxLen - 1, &ok, &tooLong);
        if (tooLong) {
          LOG_ERR("CPS", "Oversized obfuscated value for key '%s'", info.key);
          needsResave = true;
        }
        if (ok && !decoded.empty()) {
          copyToField(destPtr, decoded.c_str(), info.stringMaxLen);
          loaded = true;
        }
      }
      if (!loaded) {
        // Read as const char*, never `| std::string(...)`: ArduinoJson's
        // std::string converter drags a per-TU copy of the serializer into
        // flash. See the note in PersistableStore.h.
        const char* raw = doc[info.key].is<const char*>() ? doc[info.key].as<const char*>() : nullptr;
        if (raw) {
          // Obfuscated field recovered from a legacy plaintext value -> resave.
          if (info.obfuscated && strcmp(raw, destPtr) != 0) needsResave = true;
          copyToField(destPtr, raw, info.stringMaxLen);
        }
      }
    } else {
      const uint8_t fieldDefault = s.*(info.valuePtr);  // struct-initializer default, read before we overwrite it
      uint8_t v = doc[info.key] | fieldDefault;
      if (info.type == SettingType::ENUM) {
        v = clamp(v, (uint8_t)info.enumLabels().size(), fieldDefault);
      } else if (info.type == SettingType::TOGGLE) {
        v = clamp(v, (uint8_t)2, fieldDefault);
      } else if (info.type == SettingType::VALUE) {
        if (v < info.valueRange.min)
          v = info.valueRange.min;
        else if (v > info.valueRange.max)
          v = info.valueRange.max;
      }
      s.*(info.valuePtr) = v;
    }
  };
  forEachBaseSetting(visit);
  if (doc["pluginsEnabled"].isNull()) {
    pluginsEnabled = PluginLocations::anyPluginInstalledOnDisk() ? 1 : 0;
    needsResave = true;
  }

  if (doc["lineHeight"].isNull() && doc["lineSpacing"].is<uint8_t>()) {
    lineSpacing = readerlinespacing::fromLegacy(doc["lineSpacing"].as<uint8_t>());
    needsResave = true;
  }
  const auto normalizedFont = uint8_t(menufont::normalized(menuFontStyle));
  if (menuFontStyle != normalizedFont) needsResave = true;
  menuFontStyle = normalizedFont;

  const auto indentSpaces = doc["paragraphIndentSpaces"];
  const bool hasSavedWidth = indentSpaces.is<int>();
  const int savedWidth = hasSavedWidth ? indentSpaces.as<int>() : 0;
  paragraphIndentSpaces = migrateParagraphIndentSpaces(hasSavedWidth, savedWidth, extraParagraphSpacing != 0);
  if (!hasSavedWidth || savedWidth < 0 || savedWidth > 5) needsResave = true;

  // Older files stored one combined touch mode under "touchReaderControls":
  // 0=off, 1=tap, 2=swipe, 3=inverted tap. Split it into the master toggle
  // plus the per-direction gesture pair (the generic loop above already folded
  // out-of-range toggle values back to the On default).
  if (doc["pageTurnGesture"].isNull() && doc["previousPageGesture"].isNull() &&
      doc["touchReaderControls"].is<uint8_t>()) {
    const uint8_t mode = doc["touchReaderControls"].as<uint8_t>();
    if (mode >= 1 && mode <= 3) {
      touchReaderControls = TOUCH_READER_ON;
      pageTurnGesture = mode == 1 ? TAP_ONLY : mode == 2 ? SWIPE_ONLY : INVERTED_TAP;
      previousPageGesture = pageTurnGesture;
      needsResave = true;
    }
  }

  if (doc["sleepTimeoutMinutes"].isNull() && !doc["sleepTimeout"].isNull()) {
    const uint8_t legacyValue =
        clamp(doc["sleepTimeout"] | (uint8_t)SLEEP_10_MIN, SLEEP_TIMEOUT_COUNT, (uint8_t)SLEEP_10_MIN);
    sleepTimeoutMinutes = sleepTimeoutEnumToMinutes(legacyValue);
    needsResave = true;
  }
  // Front button remap — managed by RemapFrontButtons sub-activity, not in SettingsList.
  frontButtonBack = clamp(doc["frontButtonBack"] | (uint8_t)FRONT_HW_BACK, FRONT_BUTTON_HARDWARE_COUNT, FRONT_HW_BACK);
  frontButtonConfirm =
      clamp(doc["frontButtonConfirm"] | (uint8_t)FRONT_HW_CONFIRM, FRONT_BUTTON_HARDWARE_COUNT, FRONT_HW_CONFIRM);
  frontButtonLeft = clamp(doc["frontButtonLeft"] | (uint8_t)FRONT_HW_LEFT, FRONT_BUTTON_HARDWARE_COUNT, FRONT_HW_LEFT);
  frontButtonRight =
      clamp(doc["frontButtonRight"] | (uint8_t)FRONT_HW_RIGHT, FRONT_BUTTON_HARDWARE_COUNT, FRONT_HW_RIGHT);
  validateFrontButtonMapping(s);

  // Reader font size — an actual point size since 1.5. Files written by 1.4 and
  // earlier hold the old SMALL/MEDIUM/LARGE/EXTRA_LARGE slot in 0..3; no font is
  // renderable at those sizes, so the range is unambiguous and folds to the
  // point sizes those slots used to mean. Drop this once 1.4 upgrades are done.
  uint8_t storedFontSize = doc["fontSize"] | DEFAULT_FONT_POINT_SIZE;
  if (storedFontSize <= LEGACY_FONT_SIZE_MAX) {
    storedFontSize = 12 + storedFontSize * 2;  // 0,1,2,3 -> 12,14,16,18
    needsResave = true;
  }
  fontPointSize = storedFontSize;
  libraryView = (doc["libraryView"] | 0) == 1 ? 1 : 0;
  libraryLayout = (doc["libraryLayout"] | 0) == 1 ? 1 : 0;
  libraryClassicGrid = (doc["libraryClassicGrid"] | 1) != 0;
  libraryCollectionGrid = (doc["libraryCollectionGrid"] | 1) != 0;
  libraryClassicExpanded = (doc["libraryClassicExpanded"] | 0) != 0;
  librarySortMethod = std::min(uint8_t(doc["librarySortMethod"] | 0), uint8_t(6));
  librarySortDirections = uint8_t(doc["librarySortDirections"] | 1) & 0x7f;
  libraryExpandedList = (doc["libraryExpandedList"] | 1) != 0;
  libraryDisplay = uint8_t(doc["libraryDisplay"] | 7) & 7;
  libraryFileTypes = uint8_t(doc["libraryFileTypes"] | 15) & 15;
  weekStartsSunday = (doc["weekStartsSunday"] | 0) == 1 ? 1 : 0;
  readingFormats = uint8_t(doc["readingFormats"] | 7) & 63;
  homeLayout = uint8_t(doc["homeLayout"] | HOME_FRAMED);
  const int coverMask = doc["homeCoverLoadingMask"] | int(1u << HOME_LICORICE);
  homeCoverLoadingMask =
      coverMask >= 0 && coverMask < (1 << HOME_LAYOUT_COUNT) ? uint8_t(coverMask) : uint8_t(1u << HOME_LICORICE);
  // The retired third layout migrates to the remaining carousel.
  if (homeLayout == HOME_BOOX && (doc["homeLayoutVersion"] | 0) < 2) {
    homeLayout = HOME_CAROUSEL;
    needsResave = true;
  } else if (homeLayout >= HOME_LAYOUT_COUNT) {
    homeLayout = HOME_FRAMED;
  }
  fontWeight = clamp(doc["fontWeight"] | (uint8_t)FONT_WEIGHT_REGULAR, FONT_WEIGHT_COUNT, FONT_WEIGHT_REGULAR);

  // Font family — uses dynamic getter/setter in SettingsList so the generic loop skips it.
  const uint8_t storedFontFamily = doc["fontFamily"] | (uint8_t)0;
  // Slot zero is the Ubuntu fallback. Legacy Noto Serif selections migrate to SD below.
  fontFamily = clamp(storedFontFamily, BUILTIN_FONT_COUNT, 0);
  // These controls use board-specific display-index mappings and are skipped by
  // the generic settings loop. Persisted action IDs remain stable across boards.
  longPressMenuFunction = clamp(doc["longPressMenuFunction"] | (uint8_t)LP_MENU_DISABLED,
                                (uint8_t)(LP_MENU_CREATE_CLIPPING + 1), (uint8_t)LP_MENU_DISABLED);
  shortPwrBtn = clamp(doc["shortPwrBtn"] | (uint8_t)IGNORE, (uint8_t)SHORT_PWRBTN_COUNT, (uint8_t)IGNORE);
  homeButtonTapAction = uint8_t(HomeButtonAction::Home);
  if ((doc["homeButtonTapAction"] | uint8_t(HomeButtonAction::Home)) != uint8_t(HomeButtonAction::Home))
    needsResave = true;
  if (BoardConfig::hasHomeKey() && doc["homeButtonLongPressAction"].isNull() &&
      !doc["longPressMenuFunction"].isNull()) {
    static constexpr HomeButtonAction LEGACY[] = {HomeButtonAction::Sync,       HomeButtonAction::Ignore,
                                                  HomeButtonAction::Bookmark,   HomeButtonAction::Dictionary,
                                                  HomeButtonAction::ReaderMenu, HomeButtonAction::CreateClipping};
    if (s.longPressMenuFunction < sizeof(LEGACY) / sizeof(LEGACY[0])) {
      s.homeButtonLongPressAction = static_cast<uint8_t>(LEGACY[s.longPressMenuFunction]);
      needsResave = true;
    }
  }

  if (const char* legacyTarget = doc["homeButtonShortcutTitle"].as<const char*>())
    copyToField(homeButtonShortcutTitle, legacyTarget, sizeof(homeButtonShortcutTitle));
  if (std::strcmp(homeButtonShortcutTitle, "BESTIARY") == 0) {
    copyToField(homeButtonShortcutTitle, "READING QUEST", sizeof(homeButtonShortcutTitle));
    needsResave = true;
  }

  if (homeButtonDoubleTapAction == uint8_t(HomeButtonAction::OpenShortcut)) {
    homeButtonDoubleTapAction = uint8_t(HomeButtonAction::OpenDoubleShortcut);
    needsResave = true;
  }
  if (homeButtonLongPressAction == uint8_t(HomeButtonAction::OpenShortcut)) {
    homeButtonLongPressAction = uint8_t(HomeButtonAction::OpenLongShortcut);
    needsResave = true;
  }
  if (doc["homeButtonDoubleShortcutTitle"].isNull())
    copyToField(homeButtonDoubleShortcutTitle, homeButtonShortcutTitle, sizeof(homeButtonDoubleShortcutTitle));
  if (doc["homeButtonLongShortcutTitle"].isNull())
    copyToField(homeButtonLongShortcutTitle, homeButtonShortcutTitle, sizeof(homeButtonLongShortcutTitle));

  // SD card font family name — not in SettingsList, load manually
  const char* sfn = doc["sdFontFamilyName"] | "";
  strncpy(sdFontFamilyName, sfn, sizeof(sdFontFamilyName) - 1);
  sdFontFamilyName[sizeof(sdFontFamilyName) - 1] = '\0';
  if (storedFontFamily == LEGACY_OPENDYSLEXIC && sdFontFamilyName[0] == '\0') {
    fontFamily = NOTOSERIF;
    strncpy(sdFontFamilyName, "OpenDyslexic", sizeof(sdFontFamilyName) - 1);
    sdFontFamilyName[sizeof(sdFontFamilyName) - 1] = '\0';
    needsResave = true;
  } else if (storedFontFamily >= BUILTIN_FONT_COUNT) {
    needsResave = true;
  }
  if ((doc["readerFontStorageVersion"] | 0) < 1 && storedFontFamily == 0 && sdFontFamilyName[0] == '\0') {
    copyToField(sdFontFamilyName, "Noto Serif", sizeof(sdFontFamilyName));
    needsResave = true;
  }
  copyToField(translationDictionaryName, doc["translationDictionaryName"] | "", sizeof(translationDictionaryName));
  // Dictionary folder name — uses dynamic getter/setter in SettingsList, load manually
  copyToField(dictionaryName, doc["dictionaryName"] | "", sizeof(dictionaryName));
  // Preserve legacy named choices; previously empty selections adopt the defaults.
  dictionaryAuto = doc["dictionaryAuto"].is<bool>() ? doc["dictionaryAuto"].as<bool>() : dictionaryName[0] == '\0';
  translationDictionaryAuto = doc["translationDictionaryAuto"].is<bool>() ? doc["translationDictionaryAuto"].as<bool>()
                                                                          : translationDictionaryName[0] == '\0';
  // Language -- stored as code string for stability across enum reorders.
  if (doc["language"].is<const char*>()) {
    language = static_cast<uint8_t>(I18n::languageFromCode(doc["language"].as<const char*>()));
  }

  const char* pack = doc["languagePack"] | "";
  copyToField(languagePack, languagepack::validFilename(pack) ? pack : "", sizeof(languagePack));

  // Absent means unconfigured, which is the default.
  if (doc["keyboardLayouts"].is<uint16_t>()) {
    keyboardLayouts = doc["keyboardLayouts"].as<uint16_t>();
  }
  keyboardLanguageLayouts = 0;
  for (JsonVariantConst code : doc["keyboardLanguageLayouts"].as<JsonArrayConst>()) {
    if (!code.is<const char*>()) continue;
    for (uint8_t n = 0; n < getLanguageCount(); ++n)
      if (strcmp(code.as<const char*>(), LANGUAGE_CODES[n]) == 0) keyboardLanguageLayouts |= uint64_t{1} << n;
  }

  if (needsResave) {
    LOG_DBG("CPS", "Resaving settings to update format");
    requestResave();
  }

  if (!readerbars::readJson(doc, readerBars))
    LOG_ERR("CPS", "Invalid reader bar settings; keeping previous configuration");

  LOG_DBG("CPS", "Settings loaded from file");

  return true;
}

readerbars::Config CrossPointSettings::readerBar(readerbars::Position position) const {
  using namespace readerbars;
  if (readerBars.customized) return position == Position::Top ? readerBars.top : readerBars.bottom;
  Config config;
  if (position == Position::Top) return config;
  config.progress = static_cast<Progress>(std::min<uint8_t>(statusBarProgressBar, HIDE_PROGRESS));
  config.thickness = std::min<uint8_t>(statusBarProgressBarThickness, 2);
  if (statusBarBattery) config.slots[0] = Item::Battery;
  if (statusBarClock == STATUS_BAR_CLOCK_LEFT) config.slots[1] = Item::Clock;
  if (statusBarTitle == BOOK_TITLE) config.slots[3] = Item::Book;
  if (statusBarTitle == CHAPTER_TITLE) config.slots[3] = Item::Chapter;
  if (statusBarChapterPageCount) config.slots[4] = Item::Pages;
  if (statusBarBookProgressPercentage) config.slots[5] = Item::Percent;
  if (statusBarClock == STATUS_BAR_CLOCK_RIGHT) config.slots[6] = Item::Clock;
  return config;
}

CrossPointSettings::StatusBarSpec CrossPointSettings::statusBarSpec() const {
  StatusBarSpec spec;
  spec.showChapterPageCount = statusBarChapterPageCount != 0;
  spec.showBookProgressPercent = statusBarBookProgressPercentage != 0;
  spec.titleMode = statusBarTitle;
  spec.showBattery = statusBarBattery != 0;
  spec.showBatteryPercent = hideBatteryPercentage == HIDE_NEVER;
  spec.clockMode = statusBarClock;
  spec.clock12h = clockFormat == 1;
  spec.progressBarMode = statusBarProgressBar;
  spec.progressBarHeightPx =
      statusBarProgressBar != HIDE_PROGRESS ? static_cast<uint8_t>((statusBarProgressBarThickness + 1) * 2) : 0;
  spec.xtcMode = xtcStatusBarMode;
  return spec;
}

ReaderRenderSpec CrossPointSettings::readerRenderSpec(const uint16_t viewportWidth,
                                                      const uint16_t viewportHeight) const {
  ReaderRenderSpec spec;
  spec.engine = epubEngineFromSetting(epubEngine);
  spec.fontId = getReaderFontId();
  if (sdFontHashResolver && sdFontFamilyName[0] != '\0') {
    spec.fontContentHash = sdFontHashResolver(sdFontResolverCtx, sdFontFamilyName, fontPointSize);
  }
  spec.lineCompression = getReaderLineCompression();
  spec.characterSpacing = getCharacterSpacing();
  spec.wordSpacingPercent = wordSpacing;
  spec.extraParagraphSpacing = extraParagraphSpacing != 0;
  spec.paragraphIndentSpaces = paragraphIndentSpaces;
  spec.paragraphAlignment = paragraphAlignment;
  spec.viewportWidth = viewportWidth;
  spec.viewportHeight = viewportHeight;
  spec.hyphenationEnabled = hyphenationEnabled != 0;
  spec.embeddedStyle = embeddedStyle != 0;
  spec.imageRendering = imageRendering;
  spec.focusReadingEnabled = focusReadingEnabled != 0;
  spec.forceBold = fontWeight == FONT_WEIGHT_BOLD;
  return spec;
}

float CrossPointSettings::getReaderLineCompression() const { return readerlinespacing::multiplier(lineSpacing); }

unsigned long CrossPointSettings::getSleepTimeoutMs() const {
  if (sleepTimeoutMinutes >= SLEEP_TIMEOUT_NEVER_MINUTES) return 0UL;
  const uint8_t minutes =
      std::clamp(sleepTimeoutMinutes, MIN_SLEEP_TIMEOUT_MINUTES, static_cast<uint8_t>(SLEEP_TIMEOUT_NEVER_MINUTES - 1));
  return static_cast<unsigned long>(minutes) * 60UL * 1000UL;
}

int CrossPointSettings::getRefreshFrequency() const {
  switch (refreshFrequency) {
    case REFRESH_1:
      return 1;
    case REFRESH_5:
      return 5;
    case REFRESH_10:
      return 10;
    case REFRESH_15:
    default:
      return 15;
    case REFRESH_30:
      return 30;
    case REFRESH_NEVER:
      // Effectively disables the periodic full refresh; the page counter
      // counts down from here and never reaches the threshold in practice.
      return std::numeric_limits<int>::max();
  }
}

void CrossPointSettings::clearSdFontFamily() {
  sdFontFamilyName[0] = '\0';
  fontPointSize =
      snapToNearestPointSize(BUILTIN_READER_POINT_SIZES, std::size(BUILTIN_READER_POINT_SIZES), fontPointSize);
  saveToFile();
}

int CrossPointSettings::getReaderFontId() const {
  // Check SD card font first
  if (sdFontIdResolver) {
    const char* family = sdFontFamilyName[0] ? sdFontFamilyName : "Ubuntu";
    int id = sdFontIdResolver(sdFontResolverCtx, family, fontPointSize);
    if (id != 0) return id;
    // Fall through to built-in if SD font not found
  }

  // Missing SD resources never erase the desired family or point size.
  // Ubuntu is always resident and can render without a card font.
  return UI_12_FONT_ID;
}
