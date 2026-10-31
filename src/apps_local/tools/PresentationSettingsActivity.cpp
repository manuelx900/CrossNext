#include "PresentationSettingsActivity.h"

#include <ChromeRegion.h>
#include <CrossPointSettings.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "../../activities/util/IntervalSelectionActivity.h"
#include "../../components/UITheme.h"
#include "../../util/InterfaceThemeStyle.h"
#include "../Shelf.h"
#include "../ui/Dashboard.h"
#include "../ui/MenuFontScreen.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/SettingsArt.h"
#include "../ui/SettingsHome.h"
#include "../ui/SettingsMenuStyle.h"
#include "../ui/ThemePreviews.h"
namespace fui = freeink::ui;
namespace {
constexpr uint8_t masks[] = {1, 2, 4, 16, 32};
constexpr StrId themes[] = {StrId::STR_HOME_FRAMED, StrId::STR_HOME_FOCUS_CAROUSEL, StrId::STR_HOME_BOOX,
                            StrId::STR_HOME_LICORICE};
static_assert(sizeof(themes) / sizeof(themes[0]) == CrossPointSettings::HOME_LAYOUT_COUNT);
constexpr StrId formats[] = {StrId::STR_FORMAT_EPUB, StrId::STR_FORMAT_TXT, StrId::STR_FORMAT_MD, StrId::STR_FORMAT_XTC,
                             StrId::STR_FORMAT_IMAGES};
}  // namespace
int PresentationSettingsActivity::listCount() const {
  switch (kind) {
    case Kind::Theme:
      return CrossPointSettings::HOME_LAYOUT_COUNT + 3;
    case Kind::ReadingFormats:
      return sizeof(formats) / sizeof(formats[0]);
    case Kind::FontOverview:
    case Kind::ThemeOverview:
      return menufont::kSectionCount;
    case Kind::GameFonts:
    case Kind::AppFonts:
      return shelf::folders()[fontFolder()].count + 1;
  }
  return 0;
}
const char* PresentationSettingsActivity::screenTitle() const {
  switch (kind) {
    case Kind::Theme:
      return tr(STR_SETTINGS_APPEARANCE);
    case Kind::ReadingFormats:
      return tr(STR_READING_FORMATS);
    case Kind::FontOverview:
    case Kind::GameFonts:
    case Kind::AppFonts:
      return tr(STR_MENU_FONT);
    case Kind::ThemeOverview:
      return tr(STR_INTERFACE_THEME);
  }
  return "";
}
uint8_t& PresentationSettingsActivity::fontChoice(int row) {
  if (!contentFonts()) return row ? SETTINGS.menuFontOverrides[row] : SETTINGS.menuFontStyle;
  if (row == 0) return SETTINGS.menuFontOverrides[int(fontSection())];
  const auto item = menufont::itemFor(shelf::folders()[fontFolder()].items[row - 1].title);
  assert(item < menufont::kItemCount);
  return SETTINGS.menuFontItemOverrides[item];
}
uint8_t& PresentationSettingsActivity::themeChoice(int row) {
  if (!row) return SETTINGS.interfaceThemeStyle;
  if (row == int(menufont::Section::Home)) return SETTINGS.homeLayout;
  return SETTINGS.interfaceThemeOverrides[row];
}
void PresentationSettingsActivity::themeRow(void* context, uint16_t row, fui::ListItem& item) {
  auto& self = *static_cast<PresentationSettingsActivity*>(context);
  item = {};
  item.label = menufontui::sectionName(menufont::Section(row));
  const auto choice = self.themeChoice(row);
  item.value = row == int(menufont::Section::Home) ? I18N.get(themes[choice])
               : choice                            ? I18N.get(themes[choice - 1])
               : row                               ? tr(STR_INTERFACE_THEME_GENERAL)
                                                   : tr(STR_INTERFACE_THEME_HOME);
  item.actionValue = row;
}
const char* PresentationSettingsActivity::fontTargetName(int row) const {
  if (!contentFonts()) return menufontui::sectionName(menufont::Section(row));
  if (!row) return kind == Kind::GameFonts ? tr(STR_MENU_FONT_GAMES_MENU) : tr(STR_MENU_FONT_APPS_MENU);
  return shelf::label(shelf::folders()[fontFolder()].items[row - 1].title);
}
void PresentationSettingsActivity::fontRow(void* context, uint16_t row, fui::ListItem& item) {
  auto& self = *static_cast<PresentationSettingsActivity*>(context);
  item = {};
  item.label = self.fontTargetName(row);
  const bool general = !self.contentFonts() && row == 0;
  const auto choice = self.fontChoice(row);
  item.value = self.contentFonts() && row && choice == 0 ? tr(STR_MENU_FONT_INHERIT_MENU)
                                                         : menufontui::choiceName(general ? choice + 1 : choice);
  item.actionValue = row;
}
void PresentationSettingsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  useCompactSettingsRows();
  compactSettingsRows = kind == Kind::ReadingFormats;
  pixelui::begin(screen, saveFailed ? 80 : 0);
  if (kind == Kind::FontOverview || contentFonts() || kind == Kind::ThemeOverview) {
    pixelui::drawHeader(renderer, screenTitle(), true);
    backui::publish(true, pixelui::geometry(screen.device()).back);
    fui::ListProps props;
    props.rowProvider =
        kind == Kind::ThemeOverview ? &PresentationSettingsActivity::themeRow : &PresentationSettingsActivity::fontRow;
    props.rowProviderCtx = this;
    props.count = listCount();
    props.action = ACTION_ROW;
    props.selectedIndex = activeNav().selected;
    menufontui::styleSections(props, screen.device().width > screen.device().height);
    syncListViewport(screen, props);
    if (chrome::lightHeader && mappedInput.hasTouch() && !buttonFocus) props.selectedIndex = -1;
    pixelui::list(screen, props);
    if (saveFailed) {
      const auto safe = screen.device().safeRect();
      fui::TextStyle copy;
      copy.font = 4;
      copy.maxLines = 2;
      screen.target().text(fui::makeRect(safe.x + 16, safe.bottom() - 76, safe.width - 32, 68),
                           tr(STR_MENU_FONT_SAVE_FAILED), copy);
    }
    return;
  }
  if (kind == Kind::Theme) {
    const auto body = screen.body();
    fui::TextStyle label;
    label.font = 4;
    screen.target().text(fui::makeRect(body.x + 8, body.y + 8, body.width - 16, 32), tr(STR_HOME_THEME_LABEL), label);
    screen.setContentMarginAbsolute(fui::Insets{static_cast<int16_t>(body.y + 52),
                                                static_cast<int16_t>(screen.device().width - body.right()),
                                                static_cast<int16_t>(screen.device().height - body.bottom()), body.x});
  }
  pixelui::Header h;
  h.back = true;
  h.title = screenTitle();
  pixelui::drawHeader(renderer, h.title, true);
  backui::publish(true, pixelui::geometry(screen.device()).back);
  const int rows = kind == Kind::Theme ? CrossPointSettings::HOME_LAYOUT_COUNT : listCount();
  for (int n = 0; n < rows; ++n) {
    auto& item = items[n];
    item = {};
    item.actionValue = n;
    const auto id = kind == Kind::Theme ? themes[n] : formats[n];
    pixelui::copyNatural(labels[n], sizeof(labels[n]), I18N.get(id), pixelui::LabelCase::Preserve);
    item.label = labels[n];
    if (kind == Kind::Theme) {
      item.icon = n == 0   ? themepreview::Framed
                  : n == 1 ? themepreview::Carousel
                  : n == 2 ? themepreview::Carousel2
                           : themepreview::Bookshelf;
      item.subtitle = n == 0   ? tr(STR_HOME_FRAMED_HELP)
                      : n == 1 ? tr(STR_HOME_CAROUSEL_HELP)
                      : n == 2 ? tr(STR_HOME_BOOX_HELP)
                               : tr(STR_HOME_LICORICE_HELP);
    }
    if (kind == Kind::Theme && screen.device().width > screen.device().height) item.subtitle = nullptr;
    item.toggle = true;
    item.toggleChecked = kind == Kind::Theme ? SETTINGS.homeLayout == n : bool(SETTINGS.readingFormats & masks[n]);
  }
  fui::ListProps p;
  p.items = items;
  p.count = rows;
  p.action = ACTION_ROW;
  p.selectedIndex = activeNav().selected;
  settingsui::styleRows(p);
  if (kind == Kind::Theme) settingsui::styleAppearance(p);
  if (kind != Kind::Theme) syncListViewport(screen, p);
  if (kind == Kind::Theme) {
    const auto body = screen.body();
    const bool wide = screen.device().width > screen.device().height;
    p.toggleCheckbox = true;
    p.toggleWidth = 34;
    p.toggleHeight = 34;
    settingsui::styleThemeChoices(p, wide);
    const int height = p.rowHeight * CrossPointSettings::HOME_LAYOUT_COUNT;
    const int themeWidth = wide ? (body.width - 16) * 3 / 5 : body.width;
    screen.setContentMarginAbsolute(
        fui::Insets{body.y, static_cast<int16_t>(screen.device().width - body.x - themeWidth),
                    static_cast<int16_t>(screen.device().height - body.y - height), body.x});
    std::snprintf(labels[CrossPointSettings::HOME_LAYOUT_COUNT], sizeof(labels[CrossPointSettings::HOME_LAYOUT_COUNT]),
                  SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE ? tr(STR_HOME_SHELF_PAGES_VALUE)
                                                                           : tr(STR_HOME_RECENT_BOOK_COUNT_VALUE),
                  unsigned(SETTINGS.homeRecentBookCount));
    const auto actions = settingsui::appearanceActions(body, body.y + height, wide);
    const int themeAction = CrossPointSettings::HOME_LAYOUT_COUNT;
    const int recentAction = themeAction + 1, fontAction = themeAction + 2;
    const auto focused = [&](int index) {
      return (!mappedInput.hasTouch() || buttonFocus) && activeNav().selected == index;
    };
    dashboard::action(screen.target(), actions.themes, tr(STR_INTERFACE_THEME), &icon_murdle_face_grid_32,
                      focused(themeAction), 0);
    if (!chrome::lightHeader) screen.target().stroke(actions.themes, dashboard::ink, pixelui::kBorder);
    screen.frame().hit(actions.themes, ACTION_ROW, themeAction, fui::InputTouch);
    const auto recent = actions.recent;
    dashboard::action(screen.target(), recent, labels[CrossPointSettings::HOME_LAYOUT_COUNT], &extra_reading_book_32,
                      focused(recentAction), 0);
    if (!chrome::lightHeader) screen.target().stroke(recent, dashboard::ink, pixelui::kBorder);
    screen.frame().hit(recent, ACTION_ROW, recentAction, fui::InputTouch);
    pixelui::copyNatural(labels[CrossPointSettings::HOME_LAYOUT_COUNT + 1],
                         sizeof(labels[CrossPointSettings::HOME_LAYOUT_COUNT + 1]), tr(STR_MENU_FONT));
    if (!chrome::lightHeader)
      pixelui::copyCaps(labels[CrossPointSettings::HOME_LAYOUT_COUNT + 1],
                        sizeof(labels[CrossPointSettings::HOME_LAYOUT_COUNT + 1]), tr(STR_MENU_FONT));
    const auto font = actions.font;
    dashboard::action(screen.target(), font, labels[CrossPointSettings::HOME_LAYOUT_COUNT + 1], &icon_file_text_32,
                      focused(fontAction), 0);
    if (!chrome::lightHeader) screen.target().stroke(font, dashboard::ink, pixelui::kBorder);
    screen.frame().hit(font, ACTION_ROW, fontAction, fui::InputTouch);
    activeNav().top = 0;
    activeNav().followPending = false;
    p.topIndex = 0;
  }
  if (kind == Kind::Theme && mappedInput.hasTouch() && !buttonFocus) p.selectedIndex = -1;
  if (kind == Kind::Theme)
    settingsui::appearanceList(screen, p);
  else
    pixelui::list(screen, p);
}
void PresentationSettingsActivity::activateIndex(int n) {
  if (n < 0 || n >= listCount()) return;
  app.clearTapFlash();
  switch (kind) {
    case Kind::Theme:
      if (n == CrossPointSettings::HOME_LAYOUT_COUNT) {
        auto picker = makeUniqueNoThrow<PresentationSettingsActivity>(renderer, mappedInput, Kind::ThemeOverview);
        if (!picker) {
          LOG_ERR("SET", "OOM: interface theme settings");
          return;
        }
        startActivityForResult(std::move(picker), [this](const ActivityResult&) {
          resetUi();
          requestUpdate();
        });
        return;
      }
      if (n == CrossPointSettings::HOME_LAYOUT_COUNT + 2) {
        auto picker = makeUniqueNoThrow<PresentationSettingsActivity>(renderer, mappedInput, Kind::FontOverview);
        if (!picker) {
          LOG_ERR("SET", "OOM: menu font picker");
          return;
        }
        startActivityForResult(std::move(picker), [this](const ActivityResult&) {
          resetUi();
          requestUpdate();
        });
        return;
      }
      if (n == CrossPointSettings::HOME_LAYOUT_COUNT + 1) {
        // Reuse the bounded settings slider; no separate recent-book list is stored.
        auto picker = makeUniqueNoThrow<IntervalSelectionActivity>(
            renderer, mappedInput, "HomeRecentCount",
            SETTINGS.homeLayout == CrossPointSettings::HOME_LICORICE ? StrId::STR_HOME_SHELF_PAGES
                                                                     : StrId::STR_HOME_RECENT_BOOK_COUNT,
            SETTINGS.homeRecentBookCount, 1, 10, 1, 1);
        if (!picker) {
          LOG_ERR("SET", "OOM: recent-book count picker");
          return;
        }
        startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
          if (!result.isCancelled) {
            if (const auto* interval = std::get_if<IntervalResult>(&result.data)) {
              const auto count = uint8_t(std::clamp(int(interval->value), 1, 10));
              if (SETTINGS.homeRecentBookCount != count) {
                SETTINGS.homeRecentBookCount = count;
                SETTINGS.saveToFile();
              }
            }
          }
          requestUpdate();
        });
        return;
      }
      if (SETTINGS.homeLayout == n) return;
      SETTINGS.homeLayout = n;
      break;
    case Kind::ReadingFormats:
      SETTINGS.readingFormats ^= masks[n];
      break;
    case Kind::ThemeOverview: {
      const bool home = n == int(menufont::Section::Home);
      const int count = CrossPointSettings::HOME_LAYOUT_COUNT + (home ? 0 : 1);
      const char* names[CrossPointSettings::HOME_LAYOUT_COUNT + 1];
      if (!home) names[0] = n ? tr(STR_INTERFACE_THEME_GENERAL) : tr(STR_INTERFACE_THEME_HOME);
      for (int j = 0; j < CrossPointSettings::HOME_LAYOUT_COUNT; ++j) names[j + (home ? 0 : 1)] = I18N.get(themes[j]);
      optionPopup.show(menufontui::sectionName(menufont::Section(n)), names, count, themeChoice(n),
                       [this, n](int selected) {
                         auto& choice = themeChoice(n);
                         if (choice == selected) return;
                         const auto previous = choice;
                         choice = uint8_t(selected);
                         saveFailed = !SETTINGS.saveToFile();
                         if (saveFailed) choice = previous;
                         UITheme::getInstance().setInterfaceSection(activityManager.currentInterfaceFontSection());
                         resetUi();
                         requestUpdate();
                       });
      requestUpdate();
      return;
    }
    case Kind::FontOverview:
    case Kind::GameFonts:
    case Kind::AppFonts: {
      if (kind == Kind::FontOverview && (n == int(menufont::Section::Games) || n == int(menufont::Section::Apps))) {
        const auto child = n == int(menufont::Section::Games) ? Kind::GameFonts : Kind::AppFonts;
        auto picker = makeUniqueNoThrow<PresentationSettingsActivity>(renderer, mappedInput, child);
        if (!picker) {
          LOG_ERR("SET", "OOM: content font settings");
          return;
        }
        startActivityForResult(std::move(picker), [this](const ActivityResult&) {
          resetUi();
          requestUpdate();
        });
        return;
      }
      const bool general = kind == Kind::FontOverview && n == 0;
      const int count = general ? 3 : 4;
      const auto current =
          general ? uint8_t(menufont::normalized(fontChoice(n))) : uint8_t(menufont::normalizedOverride(fontChoice(n)));
      const int selected = general ? int(current) - 1 : current ? int(current) - 1 : 0;
      const char* names[4];
      if (!general) names[0] = contentFonts() && n ? tr(STR_MENU_FONT_INHERIT_MENU) : tr(STR_MENU_FONT_INHERIT);
      for (int j = general ? 0 : 1; j < count; ++j) names[j] = menufontui::choiceName(uint8_t(j + (general ? 2 : 1)));
      optionPopup.show(fontTargetName(n), names, count, selected, [this, n, general](int selected) {
        auto& choice = fontChoice(n);
        const auto value = uint8_t(general ? selected + 1 : selected ? selected + 1 : 0);
        if (choice == value) return;
        const auto previous = choice;
        choice = value;
        saveFailed = !SETTINGS.saveToFile();
        if (saveFailed) choice = previous;
        resetUi();
        requestUpdate();
      });
      requestUpdate();
      return;
    }
  }
  SETTINGS.saveToFile();
  if (kind == Kind::Theme) UITheme::getInstance().reload();
  resetUi();
  requestUpdate();
}

void PresentationSettingsActivity::navigateButtons() {
  const int previous = activeNav().selected;
  UiListActivity::navigateButtons();
  if (previous != activeNav().selected) buttonFocus = true;
}

void PresentationSettingsActivity::loop() {
  RenderLock lock(*this);
  if (buttonFocus && mappedInput.wasScreenTouchReleased()) {
    buttonFocus = false;
    requestUpdate();
  }
  if (optionPopup.isActive() && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    optionPopup.dismiss();
    requestUpdate();
    return;
  }
  if (optionPopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
    return;
  }
  UiListActivity::loop();
}

void PresentationSettingsActivity::render(RenderLock&& lock) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
}
