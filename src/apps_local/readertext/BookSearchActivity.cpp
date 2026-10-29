#include "BookSearchActivity.h"

#include <Memory.h>
#include <Txt.h>

#include <cctype>
#include <cstring>

#include "../../activities/util/KeyboardEntryActivity.h"
#include "../ui/PixelMenuTheme.h"
namespace fui = freeink::ui;
BookSearchActivity::SearchStream::~SearchStream() {
  if (parser) XML_ParserFree(parser);
}
bool BookSearchActivity::SearchStream::begin(const char* value) {
  if (parser) {
    if (!XML_ParserReset(parser, nullptr)) {
      failed = true;
      return false;
    }
  } else
    parser = XML_ParserCreate(nullptr);
  if (!parser) {
    LOG_ERR("BOOK-SEARCH", "OOM: XML parser");
    failed = true;
    return false;
  }
  pixelui::copyCaps(query, sizeof(query), value);
  length = std::strlen(query);
  matched = used = recentEnd = recentCount = skip = after = 0;
  found = failed = false;
  previousSpace = true;
  previousByte = 0;
  anchor[0] = matchAnchor[0] = snippet[0] = 0;
  prefix[0] = 0;
  for (int n = 1, k = 0; n < length; ++n) {
    while (k && query[n] != query[k]) k = prefix[k - 1];
    if (query[n] == query[k]) ++k;
    prefix[n] = char(k);
  }
  XML_SetUserData(parser, this);
  XML_SetElementHandler(parser, start, end);
  XML_SetCharacterDataHandler(parser, text);
  XML_SetParamEntityParsing(parser, XML_PARAM_ENTITY_PARSING_NEVER);
  return true;
}
void XMLCALL BookSearchActivity::SearchStream::start(void* context, const XML_Char* name, const XML_Char** atts) {
  auto& s = *static_cast<SearchStream*>(context);
  if (s.skip || !std::strcmp(name, "script") || !std::strcmp(name, "style") || !std::strcmp(name, "head")) ++s.skip;
  if (!s.skip && std::strcmp(name, "span"))
    for (int n = 0; atts && atts[n]; n += 2)
      if (!std::strcmp(atts[n], "id")) {
        if (std::strlen(atts[n + 1]) < sizeof(s.anchor))
          std::snprintf(s.anchor, sizeof(s.anchor), "%s", atts[n + 1]);
        else
          s.anchor[0] = 0;
      }
  if (!std::strcmp(name, "p") || !std::strcmp(name, "div") || !std::strcmp(name, "br")) text(context, " ", 1);
}
void XMLCALL BookSearchActivity::SearchStream::end(void* context, const XML_Char* name) {
  auto& s = *static_cast<SearchStream*>(context);
  if (s.skip) {
    --s.skip;
    return;
  }
  if (!std::strcmp(name, "p") || !std::strcmp(name, "div") || !std::strcmp(name, "br")) text(context, " ", 1);
}
void XMLCALL BookSearchActivity::SearchStream::text(void* context, const XML_Char* value, int size) {
  auto& s = *static_cast<SearchStream*>(context);
  if (s.skip || s.after >= 48) return;
  for (int n = 0; n < size; ++n) {
    unsigned char c = static_cast<unsigned char>(value[n]);
    const bool space = c < 128 && std::isspace(c);
    if (space && s.previousSpace) continue;
    s.previousSpace = space;
    if (space) c = ' ';
    s.recent[s.recentEnd] = char(c);
    s.recentEnd = (s.recentEnd + 1) % 128;
    s.recentCount = std::min(s.recentCount + 1, 128);
    if (s.found) {
      if (s.used + 1 < int(sizeof(s.snippet))) s.snippet[s.used++] = char(c);
      ++s.after;
      if (s.after >= 48) break;
    } else {
      unsigned char upper = c;
      if (c >= 'a' && c <= 'z')
        upper -= 32;
      else if (s.previousByte == 0xc3 && c >= 0xa0 && c <= 0xbe && c != 0xb7)
        upper -= 32;
      while (s.matched && upper != static_cast<unsigned char>(s.query[s.matched])) s.matched = s.prefix[s.matched - 1];
      if (upper == static_cast<unsigned char>(s.query[s.matched])) ++s.matched;
      if (s.matched == s.length) {
        s.found = true;
        std::snprintf(s.matchAnchor, sizeof(s.matchAnchor), "%s", s.anchor);
        int first = (s.recentEnd - s.recentCount + 128) % 128;
        while (s.recentCount > 0 && (static_cast<unsigned char>(s.recent[first]) & 0xc0) == 0x80) {
          first = (first + 1) % 128;
          --s.recentCount;
        }
        const int take = std::min(76, s.recentCount);
        first = (s.recentEnd - take + 128) % 128;
        while ((static_cast<unsigned char>(s.recent[first]) & 0xc0) == 0x80) first = (first + 1) % 128;
        for (int p = first; p != s.recentEnd && s.used < 76; p = (p + 1) % 128) s.snippet[s.used++] = s.recent[p];
      }
    }
    s.previousByte = c;
  }
  if (s.after >= 48 && s.used) {
    int first = s.used - 1;
    while (first > 0 && (static_cast<unsigned char>(s.snippet[first]) & 0xc0) == 0x80) --first;
    const auto c = static_cast<unsigned char>(s.snippet[first]);
    const int bytes = c < 0x80 ? 1 : c < 0xe0 ? 2 : c < 0xf0 ? 3 : 4;
    if (s.used - first < bytes) s.used = first;
  }
  s.snippet[s.used] = 0;
}
size_t BookSearchActivity::SearchStream::write(const uint8_t* data, size_t size) {
  if (failed || after >= 48) return 0;
  if (XML_Parse(parser, reinterpret_cast<const char*>(data), int(size), false) == XML_STATUS_ERROR) {
    failed = true;
    return 0;
  }
  return size;
}
void BookSearchActivity::onEnter() {
  ActivityResult cancelled;
  cancelled.isCancelled = true;
  setResult(std::move(cancelled));
  UiListActivity::onEnter();
  app.on(
      ACTION_USER,
      [](const fui::ActionEvent&, void* context) { static_cast<BookSearchActivity*>(context)->openQuery(); }, this);
  openQuery();
}
void BookSearchActivity::openQuery() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_READER_SEARCH), query, 64);
  if (!keyboard) {
    LOG_ERR("BOOK-SEARCH", "OOM: keyboard");
    finish();
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      if (!*query) finish();
      return;
    }
    const auto& text = std::get<KeyboardResult>(result.data).text;
    pixelui::copyCaps(query, sizeof(query), text.c_str());
    if (!*query) {
      finish();
      return;
    }
    closeInput();
    count = spine = errors = 0;
    plainText = book && Txt::isTxtOrMd(book->getPath());
    nav.reset();
    resetUi();
    searching = true;
    requestUpdate();
  });
}
void BookSearchActivity::onExit() {
  closeInput();
  if (stream.parser) {
    XML_ParserFree(stream.parser);
    stream.parser = nullptr;
  }
  UiListActivity::onExit();
}
void BookSearchActivity::closeInput() {
  input.close();
  if (textFile) textFile.close();
}
void BookSearchActivity::completeChapter(bool ok) {
  closeInput();
  if (stream.found) {
    auto& match = matches[count++];
    match.spine = uint16_t(spine);
    std::snprintf(match.anchor, sizeof(match.anchor), "%s", stream.matchAnchor);
    std::snprintf(match.snippet, sizeof(match.snippet), "%s", stream.snippet);
  } else if (!ok || stream.failed) {
    ++errors;
  }
  if (++spine >= book->getSpineItemsCount() || count == 64) searching = false;
  requestUpdate();
}
void BookSearchActivity::loop() {
  const bool leaving = mappedInput.wasReleased(MappedInputManager::Button::Back);
  UiListActivity::loop();
  if (leaving) return;
  if (!searching || !book) return;
  RenderLock lock(*this);
  if (spine >= book->getSpineItemsCount()) {
    searching = false;
    requestUpdate();
    return;
  }
  if (!input && !textFile) {
    if (!stream.begin(query)) {
      searching = false;
      ++errors;
      requestUpdate();
      return;
    }
    const bool opened = plainText ? Storage.openFileForRead("BOOK-SEARCH", book->getPath(), textFile)
                                  : book->openItemReader(book->getSpineItem(spine).href, input);
    if (!opened) {
      completeChapter(false);
      return;
    }
  }
  size_t produced = 0;
  auto status = ZipEntryReader::Status::Ok;
  if (plainText) {
    const int bytes = textFile.read(chunk, sizeof(chunk));
    if (bytes < 0)
      status = ZipEntryReader::Status::Error;
    else {
      produced = size_t(bytes);
      if (!bytes) status = ZipEntryReader::Status::Done;
      SearchStream::text(&stream, reinterpret_cast<const char*>(chunk), int(produced));
    }
  } else {
    status = input.read(chunk, sizeof(chunk), produced);
    if (produced) stream.write(chunk, produced);
  }
  if (status == ZipEntryReader::Status::Done && !plainText && !stream.found && !stream.failed &&
      XML_Parse(stream.parser, nullptr, 0, true) == XML_STATUS_ERROR)
    stream.failed = true;
  if (stream.after >= 48 || stream.failed || status != ZipEntryReader::Status::Ok)
    completeChapter(status != ZipEntryReader::Status::Error);
}
void BookSearchActivity::drawChrome() { pixelui::drawHeader(renderer, headerTitle(), true); }
void BookSearchActivity::provide(void* context, uint16_t index, fui::ListItem& item) {
  auto& s = *static_cast<BookSearchActivity*>(context);
  item = {};
  item.label = s.matches[index].snippet;
  item.actionValue = index;
}
void BookSearchActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, true);
  pixelui::begin(screen);
  const auto heading = screen.takeTop(48, 8);
  pixelui::text(screen.target(), heading, query, pixelui::kSmall);
  const auto again = screen.takeBottom(48, 8);
  searchButton = {};
  searchButton.label = tr(STR_READER_SEARCH_AGAIN);
  searchButton.action = searching ? fui::NO_ACTION : ACTION_USER;
  searchButton.styles = pixelui::buttonStyles();
  searchButton.text.font = pixelui::kSmall;
  searchButton.text.align = fui::TextAlign::Center;
  searchButton.minTouchSize = 0;
  screen.button(searchButton, again);
  const auto band = screen.takeBottom(42, 8);
  std::snprintf(status, sizeof(status), tr(STR_READER_SEARCH_PROGRESS), spine, book ? book->getSpineItemsCount() : 0);
  fui::TextStyle note;
  note.font = 3;
  note.maxLines = 2;
  screen.target().text(band,
                       !searching && errors   ? tr(STR_READER_SEARCH_INCOMPLETE)
                       : count == 64          ? tr(STR_READER_SEARCH_LIMIT)
                       : !searching && !count ? tr(STR_READER_SEARCH_NONE)
                                              : status,
                       note);
  props = {};
  pixelui::styleList(props, true);
  props.labelText.maxLines = 3;
  props.rowHeight = 84;
  props.rowProvider = provide;
  props.rowProviderCtx = this;
  props.count = count;
  props.action = ACTION_ROW;
  syncListViewport(screen, props);
  pixelui::list(screen, props);
}
void BookSearchActivity::activateIndex(int index) {
  if (index < 0 || index >= count) return;
  searching = false;
  closeInput();
  ActivityResult result;
  result.data = ChapterResult{int(matches[index].spine), matches[index].anchor};
  setResult(std::move(result));
  finish();
}
