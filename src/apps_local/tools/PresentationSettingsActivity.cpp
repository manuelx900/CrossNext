#include "PresentationSettingsActivity.h"

#include <CrossPointSettings.h>
#include <I18n.h>

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
  return kind == Kind::Theme ? sizeof(themes) / sizeof(themes[0]) : sizeof(formats) / sizeof(formats[0]);
}
void PresentationSettingsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  useCompactSettingsRows();
  compactSettingsRows = kind != Kind::Theme;
  pixelui::begin(screen);
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
  h.title = kind == Kind::Theme ? tr(STR_SETTINGS_APPEARANCE) : tr(STR_READING_FORMATS);
  pixelui::drawHeader(renderer, h.title, true);
  backui::publish(true, pixelui::geometry(screen.device()).back);
  for (int n = 0; n < listCount(); ++n) {
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
  p.count = listCount();
  p.action = ACTION_ROW;
  p.selectedIndex = activeNav().selected;
  settingsui::styleRows(p);
  if (kind == Kind::Theme) settingsui::styleAppearance(p);
  syncListViewport(screen, p);
  if (kind == Kind::Theme) {
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
      if (SETTINGS.homeLayout == n) return;
      SETTINGS.homeLayout = n;
      break;
    case Kind::ReadingFormats:
      SETTINGS.readingFormats ^= masks[n];
      break;
  }
  SETTINGS.saveToFile();
  requestUpdate();
}

void PresentationSettingsActivity::loop() {
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
    return;
  }
  UiListActivity::loop();
}
