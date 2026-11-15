#pragma once

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include "../../components/icons/BluetoothIcon.h"
#include "NavigationIcons.h"
#include "SettingsArt.h"
#include "SettingsOptionIcons.h"

namespace settingsicons {
inline freeink::ui::BitmapRef rowIcon(StrId name) {
  switch (name) {
    case StrId::STR_TEXT_SETTINGS:
      return freeink::ui::bitmapFromIcon(navicons::LettersRow64);
    case StrId::STR_READING_FORMATS:
      return freeink::ui::bitmapFromIcon(navicons::SignalRow64);
    case StrId::STR_ORIENTATION:
      return freeink::ui::bitmapFromIcon(navicons::OrientationRow64);
    case StrId::STR_READER_FOUR_ORIENTATIONS:
      return freeink::ui::bitmapFromIcon(navicons::ExpandRow64);
    case StrId::STR_IMAGES:
      return freeink::ui::bitmapFromIcon(navicons::PictureRow64);
    case StrId::STR_EPUB_ENGINE:
      return freeink::ui::bitmapFromIcon(navicons::EpubRow64);
    case StrId::STR_READER_MENU_STYLE:
      return freeink::ui::bitmapFromIcon(navicons::ReadingMenuRow64);
    case StrId::STR_WORD_DEFINITION_DICT:
      return freeink::ui::bitmapFromIcon(navicons::DefineRow64);
    case StrId::STR_WORD_TRANSLATION_DICT:
      return freeink::ui::bitmapFromIcon(navicons::TranslateRow64);

    case StrId::STR_SLEEP_SCREEN:
      return Wallpaper;
    case StrId::STR_SLEEP_COVER_MODE:
      return Fit;
    case StrId::STR_SLEEP_COVER_FILTER:
      return Filter;
    case StrId::STR_QUICK_RESUME_TIMEOUT:
      return Resume;
    case StrId::STR_SLEEP_READING_GOAL:
      return Stats;
    case StrId::STR_HIDE_BATTERY:
      return BatteryOff;
    case StrId::STR_REFRESH_FREQ:
      return Refresh;
    case StrId::STR_RESTORE_LIGHT_ON_WAKE:
      return Sun;
    case StrId::STR_NIGHT_MODE:
      return Contrast;
    case StrId::STR_QUICK_CUSTOMISE:
      return Sliders;
    case StrId::STR_HOME_BUTTON:
      return Home;
    case StrId::STR_ROTATION_DIRECTION:
      return Rotate;
    case StrId::STR_SIDE_BTN_LAYOUT:
      return Book;
    case StrId::STR_SIDE_LEFT_ACTION:
      return SideLeft;
    case StrId::STR_SIDE_RIGHT_ACTION:
      return SideRight;
    case StrId::STR_POWER_SHORT_ACTION:
      return PowerShort;
    case StrId::STR_POWER_LONG_ACTION:
      return PowerLong;
    case StrId::STR_LONG_PRESS_BEHAVIOR:
    case StrId::STR_LONG_PRESS_MENU:
      return Hold;
    case StrId::STR_POWER_WAKE_MODE:
      return Wake;
    case StrId::STR_LEFT_EDGE_LIGHT:
    case StrId::STR_RIGHT_EDGE_LIGHT:
      return Swipe;
    case StrId::STR_TOUCHSCREEN_LOCK:
      return Lock;
    case StrId::STR_TOUCH_READER_CONTROLS:
      return Touch;
    case StrId::STR_NEXT_PAGE_GESTURE:
      return PageNext;
    case StrId::STR_PREV_PAGE_GESTURE:
      return PagePrevious;
    case StrId::STR_XTC_COVER_CCW:
      return Rotate;
    case StrId::STR_SETTINGS_MANGA:
      return freeink::ui::bitmapFromIcon(navicons::MangaRow64);
    case StrId::STR_XTC_INVERT_DIRECTION:
      return Invert;
    case StrId::STR_KEYBOARD_LAYOUTS:
      return Keyboard;
    case StrId::STR_SHOW_HIDDEN_FILES:
      return FolderHidden;
    case StrId::STR_LIBRARY_USE_METADATA:
      return BookInfo;
    case StrId::STR_REMOVE_READ_FROM_RECENTS:
      return Trash;
    case StrId::STR_MOVE_FINISHED_TO_READ:
      return ReadFolder;
    case StrId::STR_CLEAR_READING_CACHE:
      return Broom;
    case StrId::STR_PREPARE_BOOKS:
      return Books;
    case StrId::STR_PREPARE_ON_OPEN:
      return BookZap;
    case StrId::STR_CROSSINK_IMPORT:
      return Stats;
    case StrId::STR_PLUGIN_SYSTEM:
      return Puzzle;
    case StrId::STR_WIFI_NETWORKS:
      return Wifi;
    case StrId::STR_BT_PAGE_TURNER:
      return Bluetooth;
    case StrId::STR_KOREADER_SYNC:
      return Sync;
    case StrId::STR_OPDS_SERVERS:
      return Servers;
    case StrId::STR_PLUGINS:
      return Extensions;
    default:
      return {};
  }
}
// Option rows share a canvas; category/home illustrations keep their native size.
inline freeink::ui::BitmapRef normalizeRowIcon(freeink::ui::BitmapRef icon) {
  struct Entry {
    const uint8_t* source;
    freeink::ui::BitmapRef normalized;
  };
  static constexpr Entry icons[] = {
      {settingsart::Book.data, settingsicons::Book},
      {settingsart::Clock.data, settingsrowart::Clock},
      {settingsart::Code.data, settingsrowart::Code},
      {settingsart::Display.data, settingsrowart::Display},
      {settingsart::Folder.data, settingsrowart::Folder},
      {settingsart::Font.data, settingsrowart::Font},
      {settingsart::Gear.data, settingsrowart::Gear},
      {settingsart::Globe.data, settingsrowart::Globe},
      {settingsart::Grid.data, settingsrowart::Grid},
      {settingsart::Hand.data, settingsrowart::Hand},
      {settingsart::Home.data, settingsicons::Home},
      {settingsart::Info.data, settingsrowart::Info},
      {settingsart::Letters.data, settingsrowart::Letters},
      {settingsart::Moon.data, settingsrowart::Moon},
      {settingsart::Orientation.data, settingsrowart::Orientation},
      {settingsart::Picture.data, settingsrowart::Picture},
      {settingsart::Puzzle.data, settingsicons::Puzzle},
      {settingsart::Sd.data, settingsrowart::Sd},
      {settingsart::Stats.data, settingsicons::Stats},
      {settingsart::Status.data, settingsrowart::Status},
      {settingsart::Translate.data, settingsrowart::Translate},
      {settingsart::Wifi.data, settingsicons::Wifi},
      {bluetoothIconBits, Bluetooth},
  };
  for (const auto& entry : icons)
    if (icon.data == entry.source) return entry.normalized;
  return icon;
}
}  // namespace settingsicons
