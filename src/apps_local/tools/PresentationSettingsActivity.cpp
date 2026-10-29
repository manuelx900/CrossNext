#include "PresentationSettingsActivity.h"

#include <CrossPointSettings.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "../../activities/util/IntervalSelectionActivity.h"
#include "../Shelf.h"
#include "../ui/Dashboard.h"
#include "../ui/MenuFontScreen.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/SettingsArt.h"
#include "../ui/SettingsHome.h"
#include "../ui/SettingsMenuStyle.h"
namespace fui = freeink::ui;
namespace {
constexpr uint8_t masks[] = {1, 2, 4, 16, 32};
constexpr StrId themes[] = {StrId::STR_HOME_FRAMED, StrId::STR_HOME_FOCUS_CAROUSEL};
static_assert(sizeof(themes) / sizeof(themes[0]) == CrossPointSettings::HOME_LAYOUT_COUNT);
constexpr StrId formats[] = {StrId::STR_FORMAT_EPUB, StrId::STR_FORMAT_TXT, StrId::STR_FORMAT_MD, StrId::STR_FORMAT_XTC,
                             StrId::STR_FORMAT_IMAGES};
}  // namespace
int PresentationSettingsActivity::listCount() const {
  switch (kind) {
    case Kind::Theme:
      return 4;
    case Kind::ReadingFormats:
      return sizeof(formats) / sizeof(formats[0]);
    case Kind::FontOverview:
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
  if (kind == Kind::FontOverview || contentFonts()) {
    pixelui::drawHeader(renderer, screenTitle(), true);
    backui::publish(true, pixelui::geometry(screen.device()).back);
    fui::ListProps props;
    props.rowProvider = &PresentationSettingsActivity::fontRow;
    props.rowProviderCtx = this;
    props.count = listCount();
    props.action = ACTION_ROW;
    props.selectedIndex = activeNav().selected;
    menufontui::styleSections(props, screen.device().width > screen.device().height);
    syncListViewport(screen, props);
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
  const int rows = kind == Kind::Theme ? 2 : listCount();
  for (int n = 0; n < rows; ++n) {
    auto& item = items[n];
    item = {};
    item.actionValue = n;
    const auto id = kind == Kind::Theme ? themes[n] : formats[n];
    pixelui::copyCaps(labels[n], sizeof(labels[n]), I18N.get(id));
    item.label = labels[n];
    if (kind == Kind::Theme) {
      item.icon = n == 0 ? settingsart::FramedPreview : settingsart::CarouselPreview;
      item.subtitle = n == 0 ? tr(STR_HOME_FRAMED_HELP) : tr(STR_HOME_CAROUSEL_HELP);
    }
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
    p.rowHeight = wide ? 88 : 140;
    p.iconSize = wide ? 72 : 108;
    const int height = p.rowHeight * 2;
    screen.setContentMarginAbsolute(fui::Insets{body.y, static_cast<int16_t>(screen.device().width - body.right()),
                                                static_cast<int16_t>(screen.device().height - body.y - height),
                                                body.x});
    std::snprintf(labels[2], sizeof(labels[2]), tr(STR_HOME_RECENT_BOOK_COUNT_VALUE),
                  unsigned(SETTINGS.homeRecentBookCount));
    const auto actions = settingsui::appearanceActions(body, body.y + height, wide);
    const auto recent = actions.recent;
    dashboard::action(screen.target(), recent, labels[2], &extra_reading_book_32, activeNav().selected == 2, 0);
    screen.target().stroke(recent, dashboard::ink, pixelui::kBorder);
    screen.frame().hit(recent, ACTION_ROW, 2, fui::InputTouch);
    pixelui::copyCaps(labels[3], sizeof(labels[3]), tr(STR_MENU_FONT));
    const auto font = actions.font;
    dashboard::action(screen.target(), font, labels[3], &icon_file_text_32, activeNav().selected == 3, 0);
    screen.target().stroke(font, dashboard::ink, pixelui::kBorder);
    screen.frame().hit(font, ACTION_ROW, 3, fui::InputTouch);
    activeNav().top = 0;
    activeNav().followPending = false;
    p.topIndex = 0;
  }
  if (kind == Kind::Theme && mappedInput.hasTouch()) p.selectedIndex = -1;
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
      if (n == 3) {
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
      if (n == 2) {
        // Reuse the bounded settings slider; no separate recent-book list is stored.
        auto picker = makeUniqueNoThrow<IntervalSelectionActivity>(renderer, mappedInput, "HomeRecentCount",
                                                                   StrId::STR_HOME_RECENT_BOOK_COUNT,
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
      const auto* ids = menufontui::choiceIds + (general ? 1 : 0);
      const int count = general ? int(menufont::Style::Count) : int(menufont::Override::Count);
      const auto current = fontChoice(n);
      // Reuse the same bounded modal as the other enum settings.
      const char* names[int(menufont::Override::Count)];
      for (int j = 0; j < count; ++j) names[j] = I18N.get(ids[j]);
      if (contentFonts() && n) names[0] = tr(STR_MENU_FONT_INHERIT_MENU);
      optionPopup.show(fontTargetName(n), names, count, current, [this, n](int selected) {
        auto& choice = fontChoice(n);
        if (choice == selected) return;
        const auto previous = choice;
        choice = uint8_t(selected);
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
  requestUpdate();
}

void PresentationSettingsActivity::loop() {
  RenderLock lock(*this);
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
