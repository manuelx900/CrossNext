#pragma once

struct SettingsFixture {
  void saveToFile() {}
} settingsFixture;
#define SETTINGS settingsFixture
class SettingsActivity {
 public:
  bool categoryHome = false;
  int mangaReturnTop = 0, mangaReturnSelection = 0, settingsCount = 0;
  int selectedCategoryIndex = 1;
  bool rotation = false, inverted = false;
  int ring = 1, rebuilds = 0, updates = 0, resets = 0, homeCalls = 0;
  std::vector<int> mangaSettings{1, 2}, readerSettings{3, 4, 5, 6};
  const std::vector<int>* currentSettings = &readerSettings;
  fui::ListNav nav;
  MappedInputManager mappedInput;
  fui::ListNav& activeNav() { return nav; }
  int ringPos() const { return ring; }
  void rebuildRowItems() { ++rebuilds; }
  void resetUi() { ++resets; }
  void requestUpdate() { ++updates; }
  void selectCategory(int category) {
    CHECK(category == 1);
    currentSettings = &readerSettings;
    settingsCount = 4;
    nav.top = 0;
  }
  void showCategoryHome() {
    ++homeCalls;
    categoryHome = true;
  }
  void onGoHome() { ++homeCalls; }
  void activateIndex(int) {}
  void toggleCurrentSetting() {
    CHECK(currentSettings == &mangaSettings);
    if (ring == 1)
      rotation = !rotation;
    else
      inverted = !inverted;
    rebuildSettingsLists();
  }
  void rebuildSettingsLists();
  void openManga();
  void closeManga();
  bool handleButtons();
};
PRODUCTION_NAV_METHODS
void SettingsActivity::rebuildSettingsLists() {
  PRODUCTION_REBUILD_ENTRY
  mangaSettings.clear();
  mangaSettings.push_back(1);
  mangaSettings.push_back(2);
  PRODUCTION_REBUILD_RETURN
}
#undef SETTINGS
static void settingsNavigation() {
  for (bool touch : {false, true}) {
    SettingsActivity a;
    a.mappedInput.touch = touch;
    a.nav.top = 2;
    a.nav.selected = 4;
    a.openManga();
    CHECK(a.currentSettings == &a.mangaSettings && a.settingsCount == 2 && a.nav.top == 0);
    CHECK(a.rebuilds == 1 && a.resets == 1);
    a.ring = 1;
    a.toggleCurrentSetting();
    CHECK(a.rotation && a.currentSettings == &a.mangaSettings && a.settingsCount == 2);
    a.ring = 2;
    a.toggleCurrentSetting();
    CHECK(a.inverted && a.currentSettings == &a.mangaSettings && a.mangaReturnTop == 2 && a.mangaReturnSelection == 4);
    a.mappedInput.back = true;
    CHECK(a.handleButtons());
    CHECK(a.currentSettings == &a.readerSettings && a.settingsCount == 4 && a.nav.top == 2);
    CHECK(a.nav.selected == (touch ? 0 : 4) && a.homeCalls == 0);
    a.mappedInput.back = false;
    a.openManga();
    a.ring = 0;
    a.mappedInput.confirm = true;
    CHECK(a.handleButtons());
    CHECK(a.currentSettings == &a.readerSettings && a.homeCalls == 0);
  }
}
