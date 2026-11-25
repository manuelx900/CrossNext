#include "BookOrbitBatchResults.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/UiListActivity.h"
#include "apps_local/library/LibraryMenu.h"
#include "apps_local/ui/Dashboard.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace bookorbit {
namespace fui = freeink::ui;
namespace {

const char* batchOutcomeLabel(BatchOutcome outcome) {
  static constexpr StrId labels[] = {StrId::STR_BOOKORBIT_BATCH_SYNCED, StrId::STR_BOOKORBIT_BATCH_SKIPPED,
                                     StrId::STR_BOOKORBIT_BATCH_FAILED};
  static_assert(std::size(labels) == BATCH_OUTCOME_COUNT);
  return I18N.get(labels[static_cast<int>(outcome)]);
}

class BatchBookListActivity final : public UiListActivity {
 public:
  BatchBookListActivity(GfxRenderer& renderer, MappedInputManager& input, const std::vector<BatchBook>& source,
                        BatchOutcome outcome)
      : UiListActivity("BookOrbitBatchBooks", renderer, input), books(source), outcome(outcome) {
    setInterfaceFontSection(menufont::Section::Files);
    for (size_t i = 0; i < books.size() && i < std::size(indices); ++i) {
      if (books[i].outcome == outcome) indices[count++] = static_cast<uint8_t>(i);
    }
  }

 private:
  const std::vector<BatchBook>& books;
  const BatchOutcome outcome;
  uint8_t indices[RecentBooksStore::MAX_RECENT_BOOKS]{};
  uint16_t count = 0;
  char rowLabel[320]{};

  int listCount() const override { return count; }

  void drawChrome() override {
    pixelui::drawHeader(renderer, batchOutcomeLabel(outcome), I18N.getLanguage() != Language::EN);
  }

  void activateIndex(int index) override { moveSelectionTo(index); }

  void buildScreen(UiScreen& screen) override {
    pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
    const auto& metrics = UITheme::getInstance().getMetrics();
    libraryui::beginFileRows(screen, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);
    if (!count) {
      screen.centeredText(tr(STR_NO_FILES_FOUND), screen.theme().bodyText);
      return;
    }

    fui::ListProps props;
    props.count = count;
    props.action = ACTION_ROW;
    props.inputMask = fui::InputTouch;
    props.rowProviderCtx = this;
    props.rowProvider = [](void* context, uint16_t index, fui::ListItem& row) {
      auto& self = *static_cast<BatchBookListActivity*>(context);
      const auto& path = self.books[self.indices[index]].path;
      const auto slash = path.find_last_of('/');
      const char* name = path.c_str() + (slash == std::string::npos ? 0 : slash + 1);
      const int length = std::snprintf(self.rowLabel, sizeof(self.rowLabel), "%u. %s", unsigned(index) + 1, name);
      if (length >= static_cast<int>(sizeof(self.rowLabel))) {
        self.rowLabel[utf8SafeTruncateBuffer(self.rowLabel, sizeof(self.rowLabel) - 1)] = '\0';
      }
      row.label = self.rowLabel;
      row.icon = listIconFor(UIIcon::Book, 32);
      row.actionValue = index;
    };
    uiTarget.setFont(4, toybox::kBookListFontId);
    libraryui::styleBookRows(props, screen.target());
    if (chrome::lightHeader) libraryui::lightRows(props);
    libraryui::fitPortraitRows(props, screen.device(), screen.body());
    props.rtl =
        I18N.getLanguage() == Language::AR || I18N.getLanguage() == Language::FA || I18N.getLanguage() == Language::HE;
    syncListViewport(screen, props);
    props.partialTrailingRow = false;
    if (chrome::lightHeader)
      fui::list(screen.frame(), screen.body(), props);
    else
      pixelui::list(screen, props, false, false, 2);
  }
};

}  // namespace

void drawBatchCards(fui::Frame<24>& frame, fui::Rect body, const BatchCounts& counts, fui::ActionId action,
                    int selection) {
  constexpr int gap = 12;
  auto& target = frame.target();
  const auto area = body.inset({0, 16, 0, 16});
  const int rowHeight = std::min(104, (area.height - 2 * gap) / BATCH_OUTCOME_COUNT);
  const int rowTop = area.y + (area.height - (rowHeight * BATCH_OUTCOME_COUNT + 2 * gap)) / 2;
  for (int index = 0; index < BATCH_OUTCOME_COUNT; ++index) {
    const auto box = fui::makeRect(area.x, rowTop + index * (rowHeight + gap), area.width, rowHeight);
    const bool active = frame.buttonSelected(action, index, selection == index);
    const auto ink = active ? lightui::paper : lightui::ink;
    lightui::surface(target, box, active ? lightui::ink : lightui::paper);
    lightui::outline(target, box, lightui::ink);
    frame.hit(box, action, index, fui::InputTouch);
    char count[12];
    std::snprintf(count, sizeof(count), "%u", counts[index]);
    const char* label = batchOutcomeLabel(static_cast<BatchOutcome>(index));
    const int countWidth = std::min(100, int(box.width) / 4);
    pixelui::text(target, fui::makeRect(box.x + 8, box.y, countWidth, box.height), count, pixelui::kTitle, ink.color,
                  fui::TextAlign::Center, true);
    pixelui::text(target,
                  fui::makeRect(box.x + countWidth + 18, box.y + 6, box.width - countWidth - 60, box.height - 12),
                  label, pixelui::kBody, ink.color);
    dashboard::chevron(target, fui::makeRect(box.right() - 42, box.y, 32, box.height), active);
  }
}

std::unique_ptr<Activity> makeBatchBookList(GfxRenderer& renderer, MappedInputManager& input,
                                            const std::vector<BatchBook>& books, BatchOutcome outcome) {
  auto activity = makeUniqueNoThrow<BatchBookListActivity>(renderer, input, books, outcome);
  if (!activity) LOG_ERR("ORBIT", "OOM: batch book list (%u bytes)", unsigned(sizeof(BatchBookListActivity)));
  return activity;
}

}  // namespace bookorbit
