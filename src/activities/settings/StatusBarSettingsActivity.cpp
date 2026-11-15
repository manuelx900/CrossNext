#include "StatusBarSettingsActivity.h"

#include <I18n.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;
namespace {
constexpr StrId ITEM_NAMES[] = {StrId::STR_NONE_OPT,
                                StrId::STR_CLOCK,
                                StrId::STR_BATTERY,
                                StrId::STR_READER_BAR_TIME_BOOK,
                                StrId::STR_READER_BAR_TIME_CHAPTER,
                                StrId::STR_CHAPTER_PAGE_COUNT,
                                StrId::STR_BOOK_PROGRESS_PERCENTAGE,
                                StrId::STR_READER_BAR_BOOK_TITLE,
                                StrId::STR_READER_BAR_CHAPTER_TITLE};
constexpr StrId SLOT_NAMES[] = {StrId::STR_READER_BAR_LEFT1,  StrId::STR_READER_BAR_LEFT2,
                                StrId::STR_READER_BAR_LEFT3,  StrId::STR_READER_BAR_CENTRE,
                                StrId::STR_READER_BAR_RIGHT1, StrId::STR_READER_BAR_RIGHT2,
                                StrId::STR_READER_BAR_RIGHT3};
constexpr StrId PROGRESS_NAMES[] = {StrId::STR_BOOK, StrId::STR_CHAPTER, StrId::STR_HIDE};
constexpr StrId THICKNESS_NAMES[] = {StrId::STR_PROGRESS_BAR_THIN, StrId::STR_PROGRESS_BAR_MEDIUM,
                                     StrId::STR_PROGRESS_BAR_THICK};
constexpr StrId XTC_NAMES[] = {StrId::STR_HIDE, StrId::STR_STATE_ON};
constexpr int PREVIEW_LABEL = 32;
}  // namespace

StatusBarSettingsActivity::StatusBarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("StatusBarSettings", renderer, mappedInput, false, true) {}

void StatusBarSettingsActivity::onEnter() { UiListActivity::onEnter(); }

readerbars::Position StatusBarSettingsActivity::position() const {
  return view == View::Top ? readerbars::Position::Top : readerbars::Position::Bottom;
}

void StatusBarSettingsActivity::save(const readerbars::Config& config) {
  {
    RenderLock lock;
    const auto before = SETTINGS.readerBar(position());
    if (before.slots == config.slots && before.progress == config.progress && before.thickness == config.thickness &&
        before.visible == config.visible)
      return;
    if (!SETTINGS.readerBars.customized) {
      const auto top = SETTINGS.readerBar(readerbars::Position::Top);
      const auto bottom = SETTINGS.readerBar(readerbars::Position::Bottom);
      SETTINGS.readerBars.top = top;
      SETTINGS.readerBars.bottom = bottom;
      SETTINGS.readerBars.customized = true;
    }
    (position() == readerbars::Position::Top ? SETTINGS.readerBars.top : SETTINGS.readerBars.bottom) = config;
  }
  SETTINGS.saveToFile();
}

bool StatusBarSettingsActivity::handleCustomInput() {
  return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); });
}

void StatusBarSettingsActivity::onBackButton() {
  if (view == View::Overview) {
    finish();
    return;
  }
  view = View::Overview;
  nav = {};
  requestUpdate();
}

void StatusBarSettingsActivity::activateIndex(int index) {
  if (optionPopup.isActive()) return;
  app.clearTapFlash();
  if (view == View::Overview) {
    switch (index) {
      case 0:
        view = View::Top;
        nav = {};
        break;
      case 1:
        view = View::Bottom;
        nav = {};
        break;
      case 2:
        optionPopup.show(
            StrId::STR_XTC_STATUS_BAR, XTC_NAMES, 2, SETTINGS.xtcStatusBarMode ? 1 : 0, [this](int choice) {
              const uint8_t next =
                  choice ? CrossPointSettings::XTC_STATUS_BAR_BOTTOM : CrossPointSettings::XTC_STATUS_BAR_HIDE;
              if (SETTINGS.xtcStatusBarMode != next) {
                SETTINGS.xtcStatusBarMode = next;
                SETTINGS.saveToFile();
              }
              requestUpdate();
            });
        break;
      case 3:
        if (SETTINGS.readerBars.customized) {
          SETTINGS.readerBars = {};
          SETTINGS.saveToFile();
        }
        break;
      default:
        break;
    }
  } else {
    auto config = SETTINGS.readerBar(position());
    if (index == 0) {
      config.visible = !config.visible;
      save(config);
    } else if (index >= 1 && index <= 7) {
      const auto slot = static_cast<unsigned>(index - 1);
      optionPopup.show(SLOT_NAMES[slot], ITEM_NAMES, static_cast<int>(readerbars::Item::Count),
                       static_cast<int>(config.slots[slot]), [this, slot](int choice) {
                         auto next = SETTINGS.readerBar(position());
                         next.slots[slot] = static_cast<readerbars::Item>(choice);
                         save(next);
                         requestUpdate();
                       });
    } else if (index == 8) {
      optionPopup.show(StrId::STR_PROGRESS_BAR, PROGRESS_NAMES, 3, static_cast<int>(config.progress),
                       [this](int choice) {
                         auto next = SETTINGS.readerBar(position());
                         next.progress = static_cast<readerbars::Progress>(choice);
                         save(next);
                         requestUpdate();
                       });
    } else if (index == 9) {
      optionPopup.show(StrId::STR_PROGRESS_BAR_THICKNESS, THICKNESS_NAMES, 3, config.thickness, [this](int choice) {
        auto next = SETTINGS.readerBar(position());
        next.thickness = static_cast<uint8_t>(choice);
        save(next);
        requestUpdate();
      });
    }
  }
  requestUpdate();
}

void StatusBarSettingsActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  useCompactSettingsRows();
  const auto top = SETTINGS.readerBar(readerbars::Position::Top);
  const auto bottom = SETTINGS.readerBar(readerbars::Position::Bottom);
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int preview = readerbars::barHeight(top) + readerbars::barHeight(bottom) + PREVIEW_LABEL + 12;
  pixelui::begin(screen, preview + (mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight));
  for (int i = 0; i < listCount(); ++i) {
    rows[i] = {};
    values[i][0] = 0;
    rows[i].actionValue = static_cast<int16_t>(i);
    if (view == View::Overview) {
      constexpr StrId names[] = {StrId::STR_READER_BAR_TOP, StrId::STR_READER_BAR_BOTTOM, StrId::STR_XTC_STATUS_BAR,
                                 StrId::STR_READER_BAR_RESTORE};
      rows[i].label = I18N.get(names[i]);
      if (i == 2)
        std::snprintf(values[i], sizeof(values[i]), "%s", I18N.get(XTC_NAMES[SETTINGS.xtcStatusBarMode ? 1 : 0]));
    } else {
      const auto& config = view == View::Top ? top : bottom;
      if (i == 0) {
        rows[i].label = tr(STR_READER_BAR_VISIBLE);
        GUI.setCheckboxRow(rows[i], config.visible);
      } else if (i <= 7) {
        rows[i].label = I18N.get(SLOT_NAMES[i - 1]);
        std::snprintf(values[i], sizeof(values[i]), "%s",
                      I18N.get(ITEM_NAMES[static_cast<unsigned>(config.slots[i - 1])]));
      } else if (i == 8) {
        rows[i].label = tr(STR_PROGRESS_BAR);
        std::snprintf(values[i], sizeof(values[i]), "%s",
                      I18N.get(PROGRESS_NAMES[static_cast<unsigned>(config.progress)]));
      } else {
        rows[i].label = tr(STR_PROGRESS_BAR_THICKNESS);
        std::snprintf(values[i], sizeof(values[i]), "%s", I18N.get(THICKNESS_NAMES[config.thickness]));
      }
    }
    rows[i].subtitle = values[i][0] ? values[i] : nullptr;
  }
  fui::ListProps props;
  props.items = rows;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  pixelui::list(screen, props);
}

void StatusBarSettingsActivity::render(RenderLock&&) {
  if (optionPopup.processRender(renderer, mappedInput)) return;
  renderer.clearScreen();
  const StrId title = view == View::Overview ? StrId::STR_CUSTOMISE_STATUS_BAR
                      : view == View::Top    ? StrId::STR_READER_BAR_TOP
                                             : StrId::STR_READER_BAR_BOTTOM;
  pixelui::drawHeader(renderer, I18N.get(title), I18N.getLanguage() != Language::EN);
  renderUi();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int hints = mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight;
  const auto top = SETTINGS.readerBar(readerbars::Position::Top);
  const auto bottom = SETTINGS.readerBar(readerbars::Position::Bottom);
  const int previewHeight = readerbars::barHeight(top) + readerbars::barHeight(bottom);
  readerbars::Data sample;
  sample.book = tr(STR_EXAMPLE_BOOK);
  sample.chapter = tr(STR_EXAMPLE_CHAPTER);
  sample.percent = 42;
  sample.page = 8;
  sample.pages = 32;
  sample.timeBook = 5400;
  sample.timeChapter = 720;
  int bezelTop, bezelRight, bezelBottom, bezelLeft;
  renderer.getOrientedViewableTRBL(&bezelTop, &bezelRight, &bezelBottom, &bezelLeft);
  const int footerTop = renderer.getScreenHeight() - bezelBottom - hints - previewHeight - 8;
  readerbars::draw(renderer, top, readerbars::Position::Top, sample, previewWorkspace, footerTop - bezelTop);
  readerbars::draw(renderer, bottom, readerbars::Position::Bottom, sample, previewWorkspace, hints);
  renderer.drawCenteredText(UI_10_FONT_ID, footerTop - PREVIEW_LABEL, tr(STR_PREVIEW));
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
