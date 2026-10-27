#include "DictionaryDefinitionActivity.h"

#include <Epub/htmlEntities.h>
#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "CrossPointSettings.h"
#include "HapticFeedback.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "apps_local/words/DictionaryHtml.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/DictHtmlPages.h"
#include "util/HtmlToPlainText.h"

namespace {

// Longest measurable/drawable span. Wrapped lines stay under the screen width
// (far below this); only pathological unbreakable tokens are split at this cap.
constexpr size_t MAX_LINE_BYTES = 191;
constexpr float PLAIN_TEXT_LINE_SPACING = 1.2f;

// Styled-path ceiling: the laid-out Pages keep the whole definition resident
// (TextBlock arenas ≈ text + ~7 bytes/word plus per-line objects), roughly
// doubling the string's footprint while this activity is stacked over the
// reader and word-select. Bigger definitions take the span-based plain-text
// path, which holds no per-page copies.
constexpr size_t MAX_STYLED_HTML_BYTES = 16 * 1024;

int dictionaryFont(const GfxRenderer& renderer, const char* text, const bool styled) {
  const int preferred = SETTINGS.getReaderFontId();
  static constexpr int SIZES[] = {12, 14, 16, 18};
  static constexpr int IDS[] = {NOTOSERIF_12_FONT_ID, NOTOSERIF_14_FONT_ID, NOTOSERIF_16_FONT_ID, NOTOSERIF_18_FONT_ID};
  size_t nearest = 0;
  for (size_t n = 1; n < std::size(SIZES); ++n)
    if (std::abs(SIZES[n] - int(SETTINGS.fontPointSize)) < std::abs(SIZES[nearest] - int(SETTINGS.fontPointSize)))
      nearest = n;
  const int fallback = IDS[nearest];
  if (preferred == fallback) return preferred;
  const auto& fonts = renderer.getFontMap();
  const auto primary = fonts.find(preferred), alternate = fonts.find(fallback);
  if (primary == fonts.end() || alternate == fonts.end()) return preferred;
  const auto* cursor = reinterpret_cast<const uint8_t*>(text);
  bool suppliesMissing = false;
  uint32_t cp;
  while ((cp = utf8NextCodepoint(&cursor))) {
    if (styled && cp == '&') {
      const auto* entity = cursor - 1;
      size_t length = 1;
      while (length < 32 && entity[length] && entity[length] != ';') ++length;
      if (length < 32 && entity[length] == ';') {
        if (const char* decoded = lookupHtmlEntity(reinterpret_cast<const char*>(entity), length + 1)) {
          const auto* value = reinterpret_cast<const uint8_t*>(decoded);
          cp = utf8NextCodepoint(&value);
          cursor = entity + length + 1;
        } else if (entity[1] == '#') {
          size_t n = 2;
          uint32_t base = 10, value = 0;
          if (entity[n] == 'x' || entity[n] == 'X') {
            base = 16;
            ++n;
          }
          const size_t firstDigit = n;
          for (; n < length; ++n) {
            const uint8_t c = entity[n];
            const uint32_t digit = c >= '0' && c <= '9'                 ? c - '0'
                                   : base == 16 && c >= 'a' && c <= 'f' ? c - 'a' + 10
                                   : base == 16 && c >= 'A' && c <= 'F' ? c - 'A' + 10
                                                                        : base;
            if (digit >= base || value > (0x10FFFF - digit) / base) break;
            value = value * base + digit;
          }
          if (n == length && n > firstDigit && value && !(value >= 0xD800 && value <= 0xDFFF)) {
            cp = value;
            cursor = entity + length + 1;
          }
        }
      }
    }
    if (cp < 0x80) continue;
    for (uint8_t style = 0; style < (styled ? 4 : 1); ++style) {
      const auto role = static_cast<EpdFontFamily::Style>(style);
      const bool original = primary->second.hasCodepoint(cp, role);
      const bool replacement = alternate->second.hasCodepoint(cp, role);
      if (original && !replacement) return preferred;
      suppliesMissing |= !original && replacement;
    }
  }
  return suppliesMissing ? fallback : preferred;
}

}  // namespace

void DictionaryDefinitionActivity::offerVocabulary(std::unique_ptr<vocabulary::Context> value, const char* dictionary,
                                                   const char* language) {
  source = std::move(value);
  vocabulary::copy(dictionaryName, sizeof(dictionaryName), dictionary);
  vocabulary::copy(sourceLanguage, sizeof(sourceLanguage), language);
}

void DictionaryDefinitionActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.setScreen(&DictionaryDefinitionActivity::noticeScreen, this);
  setEdgeBackEnabled(false);
  for (auto action : {dictionaryui::Back, dictionaryui::Save, dictionaryui::Previous, dictionaryui::Next}) {
    app.on(
        action,
        [](const freeink::ui::ActionEvent& event, void* p) {
          auto& self = *static_cast<DictionaryDefinitionActivity*>(p);
          self.app.clearTapFlash();
          switch (event.action) {
            case dictionaryui::Back:
              self.finish();
              break;
            case dictionaryui::Save:
              if (self.captureNotice.token.valid)
                self.undoPending = true;
              else if (self.source)
                self.savePending = true;
              break;
            case dictionaryui::Previous:
              self.turnPage(-1);
              break;
            case dictionaryui::Next:
              self.turnPage(1);
              break;
          }
        },
        this);
  }
  noticeAt = millis();
  noticeBrief = captureNotice.attempted;
  // Normalize StarDict multi-type separators so the wrap loop and the
  // C-string font APIs below both see the whole definition.
  std::replace(definition.begin(), definition.end(), '\0', '\n');
  if (htmlDefinition) vocabulary::prepareDictionaryHtml(definition);
  definitionFontId = dictionaryFont(renderer, definition.c_str(), htmlDefinition);
  if (definitionFontId != SETTINGS.getReaderFontId()) LOG_INF("DICT", "Using Noto Serif for missing dictionary glyphs");
  const char* plainTextReason = nullptr;
  if (!htmlDefinition) {
    plainTextReason = "dictionary format is not HTML";
  } else if (definition.size() > MAX_STYLED_HTML_BYTES) {
    plainTextReason = "HTML exceeds 16 KiB limit";
  } else if (!layoutHtmlPages()) {
    plainTextReason = "HTML layout failed; see DHTML/EHP error";
  }
  if (plainTextReason) {
    LOG_INF("DICT", "Plain-text definition: %s (bytes=%u)", plainTextReason, static_cast<unsigned>(definition.size()));
    if (htmlDefinition) definition = htmlToPlainText(definition);
    wrapText();
  } else {
    LOG_INF("DICT", "Styled definition: %u pages", static_cast<unsigned>(pages.size()));
  }
  requestUpdate();
}

void DictionaryDefinitionActivity::onExit() {
  Activity::onExit();
  if (auto* fcm = renderer.getFontCacheManager()) {
    fcm->releaseSdFontCaches();
  }
}

DictionaryDefinitionActivity::BodyArea DictionaryDefinitionActivity::bodyArea() const {
  const auto body = dictionaryui::layout(uiTarget.deviceContext()).body;
  return {body.x, body.y, body.width, body.height};
}

// Styled path: lay the HTML definition out through the EPUB chapter parser
// into reader-identical Pages. Retain the bounded original for an explicit
// vocabulary save; failures leave it available for the plain-text path.
bool DictionaryDefinitionActivity::layoutHtmlPages() {
  const BodyArea body = bodyArea();
  if (body.width <= 0 || body.height <= 0) {
    LOG_ERR("DHTML", "Invalid definition viewport: %dx%d", body.width, body.height);
    return false;
  }
  if (!buildDictionaryHtmlPages(renderer, definitionFontId, definition, static_cast<uint16_t>(body.width),
                                static_cast<uint16_t>(body.height), pages)) {
    return false;
  }
  // Keep the bounded original text until the user chooses Save. Styled pages
  // are at most 16 KiB; the lookup source is moved into this viewer.
  totalPages = static_cast<int>(pages.size());
  currentPage = 0;
  return true;
}

int DictionaryDefinitionActivity::measureSpan(const int fontId, const char* text, size_t len) const {
  char buf[MAX_LINE_BYTES + 1];
  len = std::min(len, MAX_LINE_BYTES);
  memcpy(buf, text, len);
  buf[len] = '\0';
  return renderer.getTextAdvanceX(fontId, buf, EpdFontFamily::REGULAR);
}

// Greedy word-wrap of `definition` into byte spans. '\n' breaks lines (blank
// lines survive as paragraph spacing; NULs from multi-type StarDict entries
// were normalized to newlines in onEnter); '\r' is dropped by treating it as
// a space at a token edge.
void DictionaryDefinitionActivity::wrapText() {
  lines.clear();
  lines.reserve(definition.size() / 32 + 8);

  const int fontId = definitionFontId;
  // SD-card fonts: merge every definition codepoint into the persistent
  // advance table up front. Otherwise each unseen codepoint measured below
  // falls back to an on-demand glyph load from SD (8-slot overflow ring).
  renderer.ensureSdCardFontReady(fontId, definition.c_str(), 0x01 /* REGULAR */);

  const BodyArea body = bodyArea();
  const int maxWidth = body.width;
  const int spaceWidth = renderer.getSpaceWidth(fontId, EpdFontFamily::REGULAR);
  const int lineHeight = renderer.getLineHeight(fontId, PLAIN_TEXT_LINE_SPACING);
  linesPerPage = std::max(1, body.height / lineHeight);

  const char* text = definition.c_str();
  const uint32_t n = static_cast<uint32_t>(definition.size());
  uint32_t lineStart = 0;
  uint32_t lineEnd = 0;  // one past the last token byte on the current line
  int lineWidth = 0;

  const auto flushLine = [&](uint32_t nextStart) {
    lines.push_back({lineStart, static_cast<uint16_t>(lineEnd - lineStart)});
    lineStart = nextStart;
    lineEnd = nextStart;
    lineWidth = 0;
  };

  uint32_t i = 0;
  while (i < n) {
    const char c = text[i];
    if (c == '\n' || c == '\0') {
      if (lineEnd > lineStart || (!lines.empty() && lines.back().len != 0))
        flushLine(i + 1);
      else
        lineStart = lineEnd = i + 1;
      i++;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r') {
      i++;
      continue;
    }

    // Token: run of non-whitespace bytes, capped at the measure buffer.
    const uint32_t tokenStart = i;
    while (i < n && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n' && text[i] != '\0' &&
           i - tokenStart < MAX_LINE_BYTES) {
      i++;
    }
    // If the byte cap cut the token mid-UTF-8-sequence, back off to the last
    // complete codepoint so measure/draw never see a partial sequence. A
    // natural stop lands on whitespace or the terminating NUL, never on a
    // continuation byte, so this is a no-op there.
    while (i - tokenStart > 1 && (text[i] & 0xC0) == 0x80) i--;
    const uint32_t tokenLen = i - tokenStart;
    const int tokenWidth = measureSpan(fontId, text + tokenStart, tokenLen);

    if (lineEnd == lineStart) {
      lineStart = tokenStart;
      lineEnd = tokenStart + tokenLen;
      lineWidth = tokenWidth;
    } else if (lineWidth + spaceWidth + tokenWidth <= maxWidth &&
               tokenStart + tokenLen - lineStart <= UINT16_MAX) {  // span len must fit Line::len
      lineEnd = tokenStart + tokenLen;
      lineWidth += spaceWidth + tokenWidth;
    } else {
      flushLine(tokenStart);
      lineEnd = tokenStart + tokenLen;
      lineWidth = tokenWidth;
    }

    // An unbreakable token wider than the screen is now alone on the line
    // (any previous content was flushed above): split it at the widest
    // fitting UTF-8 boundary and carry the remainder forward.
    while (lineWidth > maxWidth && lineEnd - lineStart > 1) {
      const uint32_t len = lineEnd - lineStart;
      uint32_t lastFit = 0;
      for (uint32_t f = 1; f <= len; f++) {
        if (f == len || (text[lineStart + f] & 0xC0) != 0x80) {  // codepoint boundary
          if (measureSpan(fontId, text + lineStart, f) > maxWidth) break;
          lastFit = f;
        }
      }
      if (lastFit == 0) {
        // Even a single over-wide glyph must make progress; consume its whole
        // UTF-8 sequence rather than splitting it into invalid fragments.
        lastFit = 1;
        while (lastFit < len && (text[lineStart + lastFit] & 0xC0) == 0x80) lastFit++;
      }
      const uint32_t rest = lineStart + lastFit;
      lineEnd = rest;
      flushLine(rest);
      lineEnd = rest + (len - lastFit);
      lineWidth = measureSpan(fontId, text + lineStart, lineEnd - lineStart);
    }
  }
  if (lineEnd > lineStart) flushLine(n);

  // Trim trailing blank lines so the last page is not empty padding.
  while (!lines.empty() && lines.back().len == 0) lines.pop_back();

  totalPages = std::max(1, (static_cast<int>(lines.size()) + linesPerPage - 1) / linesPerPage);
  currentPage = 0;
}

void DictionaryDefinitionActivity::loop() {
  RenderLock lock;
  const auto routed = routeTouch(mappedInput);
  if (source || captureNotice.attempted) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      if (captureNotice.token.valid)
        undoPending = true;
      else if (source)
        savePending = true;
    }
    if (savePending) {
      savePending = false;
      captureNotice = vocabulary::saveLookup(*source, source->surface, headword.c_str(), definition,
                                             htmlDefinition && !pages.empty(), dictionaryName, sourceLanguage);
      noticeAt = millis();
      noticeBrief = true;
      undone = false;
      resetUi();
      requestUpdate();
      return;
    }
    if (undoPending) {
      undoPending = false;
      undone = vocabulary::undoLookup(captureNotice.token);
      if (!undone) captureNotice.status = vocabulary::Capture::Failed;
      noticeAt = millis();
      noticeBrief = true;
      resetUi();
      requestUpdate();
      return;
    }
    if (noticeBrief && millis() - noticeAt > 5000) {
      noticeBrief = false;
      requestUpdate();
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Left) {
    turnPage(1);
    return;
  }
  if (swipe == MappedInputManager::SwipeDir::Right) {
    turnPage(-1);
    return;
  }
  int tx = 0, ty = 0;
  const auto body = bodyArea();
  if (!routed && routingReady() && mappedInput.wasScreenTapped(tx, ty) && tx >= body.x && tx < body.x + body.width &&
      ty >= body.y && ty < body.y + body.height) {
    turnPage(tx < body.x + body.width / 3 ? -1 : 1);
    return;
  }
  buttonNavigator.onNext([this] { turnPage(1); });
  buttonNavigator.onPrevious([this] { turnPage(-1); });
}

// Draws the current page: a styled Page when the HTML layout succeeded,
// otherwise the wrapped line spans (copied into a stack buffer for NUL
// termination). Called twice per render: once in font-cache scan mode, once
// for the real paint.
void DictionaryDefinitionActivity::drawBody(const int fontId, const int x, const int startY) const {
  if (!pages.empty()) {
    pages[currentPage]->render(renderer, fontId, x, startY);
    return;
  }
  const int lineHeight = renderer.getLineHeight(fontId, PLAIN_TEXT_LINE_SPACING);
  char buf[MAX_LINE_BYTES + 1];
  const int firstLine = currentPage * linesPerPage;
  const int lastLine = std::min(firstLine + linesPerPage, static_cast<int>(lines.size()));
  for (int i = firstLine; i < lastLine; i++) {
    if (lines[i].len == 0) continue;
    const size_t len = std::min(static_cast<size_t>(lines[i].len), MAX_LINE_BYTES);
    memcpy(buf, definition.c_str() + lines[i].start, len);
    buf[len] = '\0';
    renderer.drawText(fontId, x, startY + (i - firstLine) * lineHeight, buf);
  }
}

void DictionaryDefinitionActivity::render(RenderLock&&) {
  renderer.clearScreen();

  // Body: two-pass draw inside a prewarm scope (same pattern as the reader's
  // renderContents) so SD-card font glyphs load from SD in one batch instead
  // of one on-demand overflow read per character on every page turn.
  const int fontId = definitionFontId;
  const BodyArea body = bodyArea();
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  drawBody(fontId, body.x, body.y);  // scan pass: records codepoints only
  scope.endScanAndPrewarm();
  drawBody(fontId, body.x, body.y);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK),
                                            captureNotice.token.valid ? tr(STR_WORDS_UNDO)
                                            : source                  ? tr(STR_WORD_SAVE)
                                                                      : "",
                                            (currentPage > 0 ? "<" : ""), (currentPage + 1 < totalPages ? ">" : ""));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderUi();
  if (noticeBrief && captureNotice.attempted) {
    const bool failed =
        captureNotice.status == vocabulary::Capture::Failed || captureNotice.status == vocabulary::Capture::Rejected;
    GUI.drawPopup(renderer, undone ? tr(STR_WORDS_UNDONE) : failed ? tr(STR_WORDS_NOT_SAVED) : tr(STR_WORDS_SAVED));
    return;
  }
  renderer.displayBuffer();
}

void DictionaryDefinitionActivity::turnPage(const int delta) {
  if (!routingReady()) return;
  const int next = std::clamp(currentPage + delta, 0, totalPages - 1);
  if (next == currentPage) return;
  currentPage = next;
  requestUpdate();
}

void DictionaryDefinitionActivity::noticeScreen(UiScreen& screen, void* p) {
  auto& self = *static_cast<DictionaryDefinitionActivity*>(p);
  pixelui::FontScope fonts(self.uiTarget, self.renderer, I18N.getLanguage() != Language::EN);
  dictionaryui::Model model;
  model.word = self.headword.c_str();
  model.page = self.currentPage + 1;
  model.pages = self.totalPages;
  model.saveLabel = self.captureNotice.token.valid ? tr(STR_WORDS_UNDO) : self.source ? tr(STR_WORD_SAVE) : nullptr;
  dictionaryui::build(screen, model, self.chrome);
}
