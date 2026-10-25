#pragma once

#include <Epub/Page.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "activities/Activity.h"
#include "apps_local/words/DictionaryScreen.h"
#include "apps_local/words/VocabularyStorage.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

// Paged viewer for one dictionary definition. HTML definitions are laid out
// through the EPUB chapter parser into styled Pages; anything else (plain
// text, or HTML too damaged to parse) is word-wrapped once on entry and each
// page renders spans of the original string, so no per-line copies are held.
class DictionaryDefinitionActivity final : public Activity, protected UiAppHost {
 public:
  explicit DictionaryDefinitionActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string headword,
                                        std::string definition, bool htmlDefinition = false,
                                        vocabulary::CaptureNotice captureNotice = {})
      : Activity("DictionaryDefinition", renderer, mappedInput),
        UiAppHost(renderer),
        headword(std::move(headword)),
        definition(std::move(definition)),
        htmlDefinition(htmlDefinition),
        captureNotice(captureNotice) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override {
    finish();
    return true;
  }
  bool handleReaderTextSettingsGesture() override { return handleHomeGesture(); }
  void offerVocabulary(std::unique_ptr<vocabulary::Context>, const char* dictionary, const char* language);

 private:
  // One wrapped display line: a byte span of `definition`. Wrapping keeps
  // lines under the screen width, so uint16_t length is ample.
  struct Line {
    uint32_t start;
    uint16_t len;
  };

  // Usable body-text area between the header and the button hints.
  struct BodyArea {
    int x;
    int y;
    int width;
    int height;
  };

  BodyArea bodyArea() const;
  bool layoutHtmlPages();
  void wrapText();
  int measureSpan(int fontId, const char* text, size_t len) const;
  void drawBody(int fontId, int x, int startY) const;
  void turnPage(int delta);

  const std::string headword;
  // Not const: onEnter() normalizes embedded NULs (StarDict multi-type
  // separators) to newlines so C-string APIs see the whole text.
  std::string definition;
  const bool htmlDefinition;
  // Styled path: reader-identical Pages laid out from the HTML definition.
  // Empty means the plain-text span path below is active.
  std::vector<std::unique_ptr<Page>> pages;
  std::vector<Line> lines;
  int currentPage = 0;
  int totalPages = 1;
  int linesPerPage = 1;
  int definitionFontId = 0;
  ButtonNavigator buttonNavigator;
  vocabulary::CaptureNotice captureNotice;
  std::unique_ptr<vocabulary::Context> source;
  char dictionaryName[32]{}, sourceLanguage[32]{};
  bool savePending = false;
  unsigned long noticeAt = 0;
  bool noticeBrief = false, undoPending = false, undone = false;
  dictionaryui::Work chrome;
  static void noticeScreen(UiScreen&, void*);
};
