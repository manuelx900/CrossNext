
#include "src/apps_local/ui/QuickSettingsScreen.h"
#include "src/apps_local/ui/SettingsMenuStyle.h"
#include "src/apps_local/ui/SettingsRowIcon.h"
class MappedInputManager {
 public:
  enum class SwipeDir { None, Left, Right, Up, Down };
  enum class Button { Back, Confirm };
  SwipeDir swipe = SwipeDir::None;
  bool touch = true, back = false, confirm = false;
  bool hasTouch() const { return touch; }
  bool wasReleased(Button b) const { return b == Button::Back ? back : confirm; }
  SwipeDir wasSwipe() const { return swipe; }
};
class TextSettingsActivity {
 public:
  enum class Tab { Family, Size, Layout, Style };
  Tab tab_ = Tab::Layout;
  readertext::SheetView sheetView_ = readertext::SheetView::Controls;
  MappedInputManager mappedInput;
  struct Popup {
    bool active = false;
    template <class F>
    bool handleInput(const MappedInputManager&, F) {
      return active;
    }
  } optionPopup_;
  struct App {
    bool invalidated() const { return false; }
  } app;
  bool ready = true;
  int routes = 0, updates = 0, finished = 0, control = readertext::NoPage;
  void requestUpdate() { ++updates; }
  void finish() { ++finished; }
  bool routingReady() const { return ready; }
  struct Routed {
    bool routed = false;
  };
  Routed routeTouch(const MappedInputManager&, bool, bool, bool) {
    ++routes;
    return {true};
  }
  void sheetControl(int value) { control = value; }
  bool handleCustomInput();
};
PRODUCTION_INPUT_METHOD
#include <SettingsNavigation.generated.h>
int main(int argc, char** argv) {
  settingsNavigation();
  for (int tab = 0; tab < 4; ++tab) {
    CHECK(readertext::pageSwipeControl(tab, readertext::SheetView::Families, 1) == readertext::FontNext);
    CHECK(readertext::pageSwipeControl(tab, readertext::SheetView::Families, -1) == readertext::FontPrevious);
    CHECK(readertext::pageSwipeControl(tab, readertext::SheetView::Controls, 0) == readertext::NoPage);
    CHECK(readertext::pageSwipeControl(tab, readertext::SheetView::Style, 1) == readertext::NoPage);
  }
  CHECK(readertext::pageSwipeControl(1, readertext::SheetView::Controls, 1) == readertext::LayoutNext);
  CHECK(readertext::pageSwipeControl(1, readertext::SheetView::Controls, -1) == readertext::LayoutPrevious);
  CHECK(readertext::pageSwipeControl(0, readertext::SheetView::Controls, 1) == readertext::NoPage);
  for (int tab : {2, 3}) {
    CHECK(readertext::pageSwipeControl(tab, readertext::SheetView::Controls, 1) == readertext::SheetNext);
    CHECK(readertext::pageSwipeControl(tab, readertext::SheetView::Controls, -1) == readertext::SheetPrevious);
  }
  for (auto swipe : {MappedInputManager::SwipeDir::Left, MappedInputManager::SwipeDir::Right}) {
    TextSettingsActivity a;
    a.mappedInput.swipe = swipe;
    CHECK(a.handleCustomInput());
    CHECK(a.routes == 1 && a.finished == 0);
    CHECK(a.control ==
          (swipe == MappedInputManager::SwipeDir::Left ? readertext::LayoutNext : readertext::LayoutPrevious));
    TextSettingsActivity edge;
    edge.mappedInput.swipe = swipe;
    edge.mappedInput.back = true;
    CHECK(edge.handleCustomInput());
    CHECK(edge.finished == 0 && edge.control != readertext::NoPage);
    TextSettingsActivity closed;
    closed.mappedInput.swipe = swipe;
    closed.ready = false;
    CHECK(closed.handleCustomInput());
    CHECK(closed.control == readertext::NoPage);
    TextSettingsActivity popup;
    popup.mappedInput.swipe = swipe;
    popup.optionPopup_.active = true;
    CHECK(popup.handleCustomInput());
    CHECK(popup.routes == 0 && popup.control == readertext::NoPage);
    TextSettingsActivity family;
    family.sheetView_ = readertext::SheetView::Families;
    family.mappedInput.swipe = swipe;
    CHECK(family.handleCustomInput());
    CHECK(family.control ==
          (swipe == MappedInputManager::SwipeDir::Left ? readertext::FontNext : readertext::FontPrevious));
  }
  TextSettingsActivity back;
  back.mappedInput.back = true;
  CHECK(back.handleCustomInput());
  CHECK(back.finished == 1 && back.routes == 0);
  TextSettingsActivity buttons;
  buttons.mappedInput.touch = false;
  CHECK(!buttons.handleCustomInput());
  CHECK(buttons.routes == 0);
  const std::string output = argc > 1 ? argv[1] : ".";
  static constexpr StrId display[] = {StrId::STR_SLEEP_SCREEN,       StrId::STR_SLEEP_COVER_MODE,
                                      StrId::STR_SLEEP_COVER_FILTER, StrId::STR_QUICK_RESUME_TIMEOUT,
                                      StrId::STR_SLEEP_READING_GOAL, StrId::STR_HIDE_BATTERY,
                                      StrId::STR_REFRESH_FREQ,       StrId::STR_RESTORE_LIGHT_ON_WAKE,
                                      StrId::STR_NIGHT_MODE,         StrId::STR_QUICK_CUSTOMISE};
  static constexpr StrId controls[] = {StrId::STR_HOME_BUTTON,       StrId::STR_ROTATION_DIRECTION,
                                       StrId::STR_SIDE_BTN_LAYOUT,   StrId::STR_SIDE_LEFT_ACTION,
                                       StrId::STR_SIDE_RIGHT_ACTION, StrId::STR_POWER_SHORT_ACTION,
                                       StrId::STR_POWER_LONG_ACTION, StrId::STR_LONG_PRESS_BEHAVIOR,
                                       StrId::STR_POWER_WAKE_MODE,   StrId::STR_LEFT_EDGE_LIGHT};
  static constexpr StrId touch[] = {StrId::STR_TOUCHSCREEN_LOCK,     StrId::STR_TOUCH_READER_CONTROLS,
                                    StrId::STR_NEXT_PAGE_GESTURE,    StrId::STR_PREV_PAGE_GESTURE,
                                    StrId::STR_XTC_INVERT_DIRECTION, StrId::STR_KEYBOARD_LAYOUTS};
  static constexpr StrId library[] = {StrId::STR_SHOW_HIDDEN_FILES,        StrId::STR_LIBRARY_USE_METADATA,
                                      StrId::STR_REMOVE_READ_FROM_RECENTS, StrId::STR_MOVE_FINISHED_TO_READ,
                                      StrId::STR_CLEAR_READING_CACHE,      StrId::STR_PREPARE_BOOKS,
                                      StrId::STR_PREPARE_ON_OPEN,          StrId::STR_CROSSINK_IMPORT};
  static constexpr StrId connectivity[] = {StrId::STR_WIFI_NETWORKS, StrId::STR_BT_PAGE_TURNER,
                                           StrId::STR_KOREADER_SYNC, StrId::STR_OPDS_SERVERS, StrId::STR_PLUGINS};
  static constexpr StrId reading[] = {StrId::STR_TEXT_SETTINGS,
                                      StrId::STR_READING_FORMATS,
                                      StrId::STR_ORIENTATION,
                                      StrId::STR_READER_FOUR_ORIENTATIONS,
                                      StrId::STR_IMAGES,
                                      StrId::STR_EPUB_ENGINE,
                                      StrId::STR_SETTINGS_MANGA,
                                      StrId::STR_WORD_DEFINITION_DICT,
                                      StrId::STR_WORD_TRANSLATION_DICT,
                                      StrId::STR_CUSTOMISE_STATUS_BAR};
  static constexpr StrId system[] = {StrId::STR_HOME_MENU, StrId::STR_LANGUAGE, StrId::STR_TIME_TO_SLEEP,
                                     StrId::STR_CLOCK};
  static constexpr StrId device[] = {StrId::STR_CHECK_UPDATES, StrId::STR_SD_FIRMWARE_UPDATE, StrId::STR_ABOUT,
                                     StrId::STR_DEV_MODE};
  const StrId* groups[] = {display, controls, touch, library, connectivity, reading, system, device};
  const int counts[] = {10, 10, 6, 8, int(std::size(connectivity)), 10, 4, 4};
  const auto fallback = [](StrId id) {
    switch (id) {
      case StrId::STR_TEXT_SETTINGS:
        return settingsart::Letters;
      case StrId::STR_READING_FORMATS:
        return settingsart::Stats;
      case StrId::STR_ORIENTATION:
        return settingsart::Orientation;
      case StrId::STR_IMAGES:
        return settingsart::Picture;
      case StrId::STR_WORD_TRANSLATION_DICT:
        return settingsart::Translate;
      case StrId::STR_CUSTOMISE_STATUS_BAR:
        return settingsart::Status;
      case StrId::STR_HOME_MENU:
        return settingsart::Home;
      case StrId::STR_LANGUAGE:
        return settingsart::Globe;
      case StrId::STR_TIME_TO_SLEEP:
        return settingsart::Moon;
      case StrId::STR_CLOCK:
        return settingsart::Clock;
      case StrId::STR_CHECK_UPDATES:
        return settingsart::Wifi;
      case StrId::STR_SD_FIRMWARE_UPDATE:
        return settingsart::Sd;
      case StrId::STR_ABOUT:
        return settingsart::Info;
      case StrId::STR_DEV_MODE:
        return settingsart::Code;
      default:
        return settingsart::Book;
    }
  };
  FontBridge label(ubuntu_12_regular), small(ubuntu_10_regular);
  for (auto lang : {Language::EN, Language::ES})
    for (bool light : {false, true})
      for (bool wide : {false, true})
        for (int group = 0; group < 8; ++group) {
          I18N.setLanguage(lang);
          chrome::lightHeader = light;
          Canvas c(wide ? 800 : 480, wide ? 480 : 800);
          c.target.setFont(0, label.font);
          c.target.setFont(1, label.font);
          c.target.setFont(3, small.font);
          c.target.setFont(4, small.font);
          pixelui::Header header;
          header.title = group == 0   ? tr(STR_CAT_DISPLAY)
                         : group == 3 ? tr(STR_LIBRARY)
                         : group == 4 ? tr(STR_SETTINGS_CONNECTIVITY)
                         : group == 5 ? tr(STR_SETTINGS_READING)
                         : group == 6 ? tr(STR_CAT_SYSTEM)
                         : group == 7 ? tr(STR_SETTINGS_DEVICE)
                                      : tr(STR_CAT_CONTROLS);
          header.back = true;
          header.battery = 100;
          pixelui::header(c.target, c.device, header);
          pixelui::begin(c.screen);
          fui::ListItem items[10]{};
          for (int n = 0; n < counts[group]; ++n) {
            items[n].label = I18N.get(groups[group][n]);
            items[n].icon = settingsicons::rowIcon(groups[group][n]);
            if (!items[n].icon.data) items[n].icon = fallback(groups[group][n]);
            items[n].icon = settingsicons::normalizeRowIcon(items[n].icon);
            items[n].actionValue = n;
            items[n].value = ">";
            CHECK(items[n].icon.data && items[n].icon.width == 64);
          }
          if (group == 0) {
            items[0].sectionHeading = tr(STR_SETTINGS_SUSPENSION);
            items[4].sectionHeading = tr(STR_CAT_DISPLAY);
            items[0].value = tr(STR_DARK);
            items[1].value = tr(STR_FIT);
            items[2].value = tr(STR_NONE_OPT);
            items[5].value = tr(STR_NEVER);
            items[6].value = lang == Language::ES ? "15 pág." : "15 pages";
            for (int n : {3, 4, 7, 8}) {
              items[n].toggle = true;
              items[n].value = nullptr;
            }
          } else if (group == 1) {
            items[0].sectionHeading = tr(STR_SETTINGS_BUTTONS);
            items[9].sectionHeading = tr(STR_SETTINGS_TOUCH);
          } else if (group == 2) {
            items[0].sectionHeading = tr(STR_SETTINGS_TOUCH);
            items[5].sectionHeading = tr(STR_SETTINGS_KEYBOARD);
          } else if (group == 3) {
            items[0].sectionHeading = tr(STR_SETTINGS_FILES);
            items[2].sectionHeading = tr(STR_SETTINGS_FINISHED_BOOKS);
            items[4].sectionHeading = tr(STR_SETTINGS_MAINTENANCE);
            for (int n : {0, 1, 2, 3, 6}) {
              items[n].toggle = true;
              items[n].value = nullptr;
            }
            items[1].toggleChecked = true;
          } else if (group == 5) {
            items[0].sectionHeading = tr(STR_SETTINGS_TEXTS);
            items[2].sectionHeading = tr(STR_SETTINGS_READING);
            items[7].sectionHeading = tr(STR_SETTINGS_DICTIONARIES);
            items[9].sectionHeading = tr(STR_STATUS_BAR);
            items[3].toggle = true;
            items[3].value = nullptr;
          } else if (group == 6) {
            items[0].sectionHeading = tr(STR_SETTINGS_PERSONALISATION);
            items[2].sectionHeading = tr(STR_SETTINGS_POWER);
            items[3].sectionHeading = tr(STR_SETTINGS_DATE_TIME);
          } else if (group == 7) {
            items[0].sectionHeading = tr(STR_SETTINGS_DEVICE);
            items[3].sectionHeading = tr(STR_SETTINGS_DEVELOPMENT);
            items[3].toggle = true;
            items[3].value = nullptr;
          } else if (group == 4) {
            items[0].toggle = true;
            items[0].value = nullptr;
          }
          fui::ListProps p;
          settingsui::styleRows(p, true);
          p.items = items;
          p.count = counts[group];
          p.action = 70;
          settingsui::drawRows(c.screen, p, false);
          CHECK(p.rowStyles.normal.borderWidth == 1 && p.selectionMarker == fui::SelectionMarker::None);
          CHECK(p.count == counts[group]);
          c.checkHits();
          if (lang == Language::ES && !wide)
            c.write(output + "/settings-" + std::to_string(group) + (light ? "-light" : "-framed") + ".pbm");
        }
  for (bool light : {false, true}) {
    chrome::lightHeader = light;
    I18N.setLanguage(Language::ES);
    Canvas c(480, 800);
    c.target.setFont(0, label.font);
    c.target.setFont(3, small.font);
    pixelui::Header header;
    header.title = tr(STR_SETTINGS_MANGA);
    header.back = true;
    header.battery = 100;
    pixelui::header(c.target, c.device, header);
    pixelui::begin(c.screen);
    fui::ListItem items[2]{};
    const StrId ids[] = {StrId::STR_XTC_COVER_CCW, StrId::STR_XTC_INVERT_DIRECTION};
    for (int n = 0; n < 2; ++n) {
      items[n].label = I18N.get(ids[n]);
      items[n].icon = settingsicons::rowIcon(ids[n]);
      items[n].toggle = true;
      items[n].actionValue = n;
    }
    fui::ListProps p;
    settingsui::styleRows(p, true);
    p.items = items;
    p.count = 2;
    p.action = 70;
    settingsui::drawRows(c.screen, p, false);
    c.checkHits();
    c.write(output + "/manga-" + (light ? "light" : "framed") + ".pbm");
  }
  for (bool light : {false, true})
    for (bool wide : {false, true}) {
      chrome::lightHeader = light;
      I18N.setLanguage(Language::ES);
      Canvas c(wide ? 800 : 480, wide ? 480 : 800);
      c.target.setFont(0, label.font);
      c.target.setFont(3, small.font);
      pixelui::Header header;
      header.title = tr(STR_QUICK_CUSTOMISE);
      header.back = true;
      header.battery = 100;
      pixelui::header(c.target, c.device, header);
      pixelui::begin(c.screen);
      fui::ListItem items[static_cast<int>(quickui::Shortcut::Count)]{};
      for (int n = 0; n < static_cast<int>(quickui::Shortcut::Count); ++n) {
        const auto shortcut = static_cast<quickui::Shortcut>(n);
        items[n].label = quickui::label(shortcut);
        items[n].icon = fui::bitmapFromIcon(quickui::icon(shortcut));
        items[n].toggle = true;
        items[n].toggleChecked = shortcut == quickui::Shortcut::Dark;
        items[n].actionValue = n;
      }
      fui::ListProps props;
      settingsui::styleRows(props);
      props.items = items;
      props.count = static_cast<int>(quickui::Shortcut::Count);
      props.action = 70;
      quickui::styleConfiguration(props);
      pixelui::list(c.screen, props);
      c.checkHits();
      if (!wide) c.write(output + "/quick-config-" + (light ? "light" : "framed") + ".pbm");
      for (int variant = 0; variant < 3; ++variant) {
        Canvas panel(wide ? 800 : 480, wide ? 480 : 800);
        panel.target.setFont(0, label.font);
        panel.target.setFont(3, small.font);
        quickui::Model model;
        model.shortcuts[0] = static_cast<uint8_t>(quickui::Shortcut::Rotate);
        model.shortcuts[1] = static_cast<uint8_t>(quickui::Shortcut::Light);
        model.shortcuts[2] = static_cast<uint8_t>(quickui::Shortcut::Warm);
        model.shortcuts[3] = static_cast<uint8_t>(quickui::Shortcut::Bluetooth);
        model.rotateRight = variant == 1;
        model.lightOn = variant != 2;
        model.warmth = variant == 2 ? 0 : 50;
        quickui::Work work;
        quickui::build(panel.screen, model, work);
        panel.checkHits();
        panel.write(output + "/quick-panel-" + (light ? "light" : "framed") + (wide ? "-wide-" : "-") +
                    std::to_string(variant) + ".pbm");
      }
    }
  CHECK(!settingsicons::rowIcon(StrId::STR_NONE_OPT).data);
  std::printf("Settings icons and production sheet gestures: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
