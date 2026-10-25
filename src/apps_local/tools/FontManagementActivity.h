#pragma once
#include <I18n.h>
#include <Memory.h>

#include "../ui/PixelMenuTheme.h"
#include "SdCardFontSystem.h"
#include "activities/UiListActivity.h"
#include "activities/settings/FontDownloadActivity.h"
#include "activities/settings/TextSettingsActivity.h"
class FontManagementActivity final : public UiListActivity {
 public:
  FontManagementActivity(GfxRenderer& r, MappedInputManager& i) : UiListActivity("FontManagement", r, i, false, true) {}

 private:
  freeink::ui::ListItem rows[2]{};
  freeink::ui::ListProps props;
  int listCount() const override { return 2; }
  const char* headerTitle() const override { return tr(STR_MANAGE_FONTS); }
  void activateIndex(int n) override {
    if (n == 0) {
      auto a = makeUniqueNoThrow<TextSettingsActivity>(renderer, mappedInput, &sdFontSystem.registry());
      if (a)
        startActivityForResult(std::move(a), nullptr);
      else
        LOG_ERR("FONT", "OOM: installed fonts");
    } else {
      auto a = makeUniqueNoThrow<FontDownloadActivity>(renderer, mappedInput);
      if (a)
        startActivityForResult(std::move(a), nullptr);
      else
        LOG_ERR("FONT", "OOM: downloader");
    }
  }
  void buildScreen(UiScreen& s) override {
    pixelui::FontScope fonts(uiTarget, renderer, true);
    useCompactSettingsRows();
    pixelui::begin(s);
    rows[0] = {};
    rows[1] = {};
    rows[0].label = tr(STR_FONTS_INSTALLED);
    rows[0].subtitle = tr(STR_FONTS_LOCAL_HELP);
    rows[1].label = tr(STR_FONTS_DOWNLOAD);
    rows[1].subtitle = tr(STR_FONTS_WIFI_HELP);
    rows[1].actionValue = 1;
    props = {};
    props.items = rows;
    props.count = 2;
    props.action = ACTION_ROW;
    props.inputMask = freeink::ui::InputTouch;
    props.labelText = s.theme().smallText;
    syncListViewport(s, props);
    pixelui::list(s, props);
  }
};
