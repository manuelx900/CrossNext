#include "QuotesActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../ReaderFontSizes.h"
#include "../../SdCardFontSystem.h"
#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ReadingToolsUi.h"
#include "../ui/ToyboxFonts.h"
#include "QuoteFontSizes.h"
#include "QuotesStore.h"
#include "components/SideButtonNavigation.h"
namespace fui = freeink::ui;

namespace {
constexpr const char* builtinFontNames[] = {"Noto Serif", "Noto Sans", "Instrument Serif"};
constexpr StrId builtinFontLabels[] = {StrId::STR_NOTO_SERIF, StrId::STR_NOTO_SANS, StrId::STR_RT_INSTRUMENT_SERIF};
static_assert(std::size(builtinFontNames) == quotes::kBuiltinFamilies &&
              std::size(builtinFontLabels) == quotes::kBuiltinFamilies);

const char* fontLabel(uint8_t family, const char* installed) {
  return *installed ? installed : I18N.get(builtinFontLabels[family]);
}

bool isBuiltinName(const std::string& name) {
  return std::any_of(std::begin(builtinFontNames), std::end(builtinFontNames),
                     [&](const char* builtin) { return name == builtin; });
}

std::vector<uint8_t> quotePointSizes(uint8_t family, const char* installed) {
  const auto& registry = sdFontSystem.registry();
  const char* name = *installed ? installed : builtinFontNames[family];
  const auto* info = registry.findFamily(name);
  auto points = !info          ? std::vector<uint8_t>{}
                : info->vector ? readerFontPointSizes(&registry, name)
                               : info->availableSizes();
  if (!*installed || points.empty()) {
    points.reserve(points.size() + std::size(quotes::kBuiltinPoints[family]));
    for (const uint8_t pt : quotes::kBuiltinPoints[family]) {
      if (!pt) continue;
      const auto at = std::lower_bound(points.begin(), points.end(), pt);
      if (at == points.end() || *at != pt) points.insert(at, pt);
    }
  }
  return points;
}
}  // namespace

int QuotesActivity::listCapacity(const fui::DeviceContext& device) const {
  return quotesui::capacity(device, tab == 1 && !selectedBook);
}

std::unique_ptr<Activity> QuotesActivity::create(GfxRenderer& r, MappedInputManager& i) {
  auto app = makeUniqueNoThrow<QuotesActivity>(r, i);
  if (!app) LOG_ERR("QUOTES", "OOM: %u bytes", unsigned(sizeof(QuotesActivity)));
  return app;
}
void QuotesActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  savedDateFormat = quotesui::savedDateFormat();
  if (Storage.exists("/My Clippings.txt")) {
    scanning = Storage.openFileForRead("QUOTES", "/My Clippings.txt", source);
    failed = !scanning;
    if (scanning) {
      const auto size = source.fileSize64();
      if (size > UINT32_MAX) {
        scanning = false;
        failed = true;
      } else {
        scanLength = uint32_t(size);
      }
    }
  }
  requestUpdate();
}
void QuotesActivity::onExit() {
  if (temporaryFont) sdFontSystem.ensureLoaded(renderer);
  source.close();
  Activity::onExit();
}
void QuotesActivity::fail() {
  LOG_ERR("QUOTES", "Quote file or state unavailable");
  failed = true;
  view = View::Notice;
  interactionsReady = false;
  requestUpdate();
}
bool QuotesActivity::readQuote(int slot) {
  if (slot < 0 || slot >= count || !source.seekSet(entries[slot].start)) return false;
  const auto want = std::min(size_t(entries[slot].end - entries[slot].start), sizeof(record) - 1);
  if (source.read(record, want) != int(want)) return false;
  record[want] = 0;
  quotes::parse(record, quote);
  quoteGlyphsReady = false;
  paragraph.pages = 0;
  return true;
}
void QuotesActivity::recordQuote() {
  const uint32_t end = scanOffset;
  const int slot = count < quotes::kMax ? (oldest + count++) % quotes::kMax : oldest;
  if (count == quotes::kMax && entries[slot].end) oldest = (oldest + 1) % quotes::kMax;
  auto& entry = entries[slot];
  entry = {};
  entry.start = nextStart;
  entry.end = end;
  entry.key = scanHash;
  for (int n = 0; n < 4; ++n) entry.key = quotes::hashByte(entry.key, uint8_t(nextStart >> (n * 8)));
  record[scanHeaderBytes] = 0;
  quotes::parse(record, quote, true);
  entry.book = quotes::hashByte(quotes::hash(quote.title), 0);
  for (const char* p = quote.author; *p; ++p) entry.book = quotes::hashByte(entry.book, uint8_t(*p));
  entry.page = std::clamp(quote.page, 0, 65535);
  scanHeaderBytes = scanHeaderLines = 0;
  nextStart = end;
  scanHash = quotes::kHashStart;
}
void QuotesActivity::rebuild() {
  visibleCount = bookCount = favoriteCount = total = bookTotal = bookFavorites = 0;
  const bool grouping = tab == 1 && !selectedBook;
  for (int rank = 0; rank < count; ++rank) {
    const int slot = (oldest + count - 1 - rank) % quotes::kMax;
    const auto& e = entries[slot];
    if (e.flags & quotes::kHidden) continue;
    const bool favorite = e.flags & quotes::kFavorite;
    ++total;
    if (favorite) ++favoriteCount;
    if (grouping) {
      int n = 0;
      while (n < bookCount && entries[groups[n]].book != e.book) ++n;
      if (n == bookCount) groups[bookCount++] = slot;
    }
    if (selectedBook) {
      if (e.book != selectedBook) continue;
      ++bookTotal;
      if (favorite) ++bookFavorites;
      if (!bookFavoritesOnly || favorite) visible[visibleCount++] = slot;
    } else if (tab == 0 || (tab == 2 && favorite)) {
      visible[visibleCount++] = slot;
    }
  }
  if (grouping) {
    visibleCount = bookCount;
    std::copy(groups, groups + bookCount, visible);
  }
  if (oldFirst) {
    if (selectedBook)
      std::sort(visible, visible + visibleCount,
                [&](uint16_t a, uint16_t b) { return entries[a].page < entries[b].page; });
    else
      std::reverse(visible, visible + visibleCount);
  }
  selection = std::clamp(selection, 0, std::max(0, visibleCount - 1));
}
void QuotesActivity::refresh(bool rebuildIndex, bool repaint) {
  if (rebuildIndex) rebuild();
  cachedCapacity = listCapacity(toybox::makeTarget(renderer).deviceContext());
  if (!visibleCount && (view == View::Detail || view == View::Confirm)) view = View::List;
  if (view == View::Detail || view == View::Confirm) {
    if (visibleCount && !readQuote(visible[selection])) fail();
    return;
  }
  const int cap = cachedCapacity;
  const int pages = std::max(1, (visibleCount + cap - 1) / cap);
  listPage = std::clamp(listPage, 0, pages - 1);
  rowCount = 0;
  for (int n = listPage * cap; n < std::min(visibleCount, (listPage + 1) * cap); ++n) {
    const int slot = visible[n];
    auto& r = rows[rowCount++];
    r = {};
    if (!readQuote(slot)) {
      fail();
      return;
    }
    readingui::copyText(r.title, sizeof(r.title), quote.title);
    readingui::copyText(r.author, sizeof(r.author), quote.author);
    readingui::copyText(r.preview, sizeof(r.preview), quote.body);
    r.index = n;
    r.page = quote.page;
    if (tab == 1 && !selectedBook) {
      for (int k = 0; k < count; ++k)
        if (!(entries[k].flags & quotes::kHidden) && entries[k].book == entries[slot].book) ++r.count;
    }
  }
  interactionsReady = false;
  if (repaint) requestUpdate();
}
void QuotesActivity::favorite() {
  if (selection < 0 || selection >= visibleCount || !stateWritable) {
    fail();
    return;
  }
  auto& flags = entries[visible[selection]].flags;
  flags ^= quotes::kFavorite;
  if (!quotes::saveFlags(entries, count)) {
    flags ^= quotes::kFavorite;
    fail();
    return;
  }
  // A quote removed from Favorites falls out of that list. Return to the
  // filtered list instead of switching the text beneath the reader.
  const bool marked = flags & quotes::kFavorite;
  if (!marked && (tab == 2 || (selectedBook && bookFavoritesOnly))) {
    view = View::List;
    listPage = selection / std::max(1, cachedCapacity);
    refresh();
  } else {
    const int change = marked ? 1 : -1;
    favoriteCount += change;
    if (selectedBook) bookFavorites += change;
  }
  interactionsReady = false;
  requestUpdate();
}
void QuotesActivity::turnText(int step) {
  if (view != View::Detail) return;
  const int next = std::clamp(bodyPage + step, 0, bodyPages - 1);
  if (next == bodyPage) return;
  bodyPage = next;
  interactionsReady = false;
  requestUpdate();
}
void QuotesActivity::turnQuote(int step) {
  if (view != View::Detail || !visibleCount) return;
  const int next = std::clamp(selection + step, 0, visibleCount - 1);
  if (next == selection) return;
  selection = next;
  bodyPage = 0;
  refresh(false);
  interactionsReady = false;
  requestUpdate();
}
void QuotesActivity::turn(int step) {
  if (view == View::Detail) {
    if (bodyPages > 1)
      turnText(step);
    else
      turnQuote(step);
    return;
  }
  if (view != View::List) return;
  const int cap = std::max(1, cachedCapacity);
  const int pages = std::max(1, (visibleCount + cap - 1) / cap);
  const int next = std::clamp(listPage + step, 0, pages - 1);
  if (next == listPage) return;
  listPage = next;
  refresh(false);
}
void QuotesActivity::back() {
  if (view == View::Notice) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (view == View::Confirm || view == View::TextSettings) {
    view = view == View::TextSettings ? textSettingsOrigin : View::Detail;
    navigation.clear();
    interactionsReady = false;
    requestUpdate();
    return;
  }
  if (view == View::Detail) {
    view = View::List;
    listPage = selection / listCapacity(toybox::makeTarget(renderer).deviceContext());
    refresh(false);
    return;
  }
  if (selectedBook) {
    selectedBook = 0;
    bookFavoritesOnly = false;
    oldFirst = false;
    listPage = 0;
    refresh();
    return;
  }
  shelf::leave(renderer, mappedInput);
}

void QuotesActivity::updateQuoteFont() {
  const auto family = SETTINGS.quoteTextFamily;
  auto pt = SETTINGS.quoteTextPointSize;
  sdFontSystem.refreshIfDirty();
  const char* installed = SETTINGS.quoteSdFontFamily;
  if (*installed && !sdFontSystem.registry().findFamily(installed)) installed = "";
  const auto& builtin = quotes::kBuiltinPoints[family];
  const int builtinCount = builtin[2] ? 3 : 2;
  const auto builtInSize = [&] { return std::binary_search(builtin, builtin + builtinCount, pt); };
  if (*installed || !builtInSize()) pt = snapToNearestPointSize(quotePointSizes(family, installed), pt);
  if (*installed || !builtInSize()) {
    const char* name = *installed ? installed : builtinFontNames[family];
    quoteFontId = sdFontSystem.loadTemporaryFont(renderer, name, pt, /* requireBundledDefaults */ false);
    temporaryFont = true;
    if (quoteFontId) return;
    pt = snapToNearestPointSize(builtin, builtinCount, pt);
  }
  if (temporaryFont) {
    sdFontSystem.ensureLoaded(renderer);
    temporaryFont = false;
  }
  const bool small = pt <= builtin[0];
  switch (static_cast<quotes::FontFamily>(family)) {
    case quotes::FontFamily::NotoSerif:
      quoteFontId = small ? toybox::kReadingSmallFontId : toybox::kReadingFontId;
      break;
    case quotes::FontFamily::NotoSans:
      quoteFontId = toybox::sansReadingFont(renderer, small);
      break;
    case quotes::FontFamily::InstrumentSerif:
      quoteFontId = small              ? toybox::kSerifSmallFontId
                    : pt >= builtin[2] ? toybox::kSerifTitleFontId
                                       : toybox::kSerifTileFontId;
      break;
  }
}

void QuotesActivity::pickFont() {
  sdFontSystem.refreshIfDirty();
  const auto& families = sdFontSystem.registry().getFamilies();
  std::vector<std::string> labels;
  labels.reserve(quotes::kBuiltinFamilies + families.size());
  for (const StrId label : builtinFontLabels) labels.emplace_back(I18N.get(label));
  int selected = settingsFontFamily;
  for (const auto& family : families) {
    if (isBuiltinName(family.name)) continue;
    if (family.name == settingsSdFont) selected = static_cast<int>(labels.size());
    labels.push_back(family.name);
  }
  options.show(StrId::STR_FONT, std::move(labels), selected, [this](int choice) {
    if (choice < 0) return;
    if (choice < quotes::kBuiltinFamilies) {
      settingsFontFamily = uint8_t(choice);
      settingsSdFont[0] = 0;
    } else {
      readingui::copyText(settingsSdFont, sizeof(settingsSdFont), options.selectedLabel());
    }
    textSettings.pointSize =
        snapToNearestPointSize(quotePointSizes(settingsFontFamily, settingsSdFont), textSettings.pointSize);
    textSettings.saveFailed = false;
    interactionsReady = false;
  });
}

void QuotesActivity::pickSize() {
  auto points = quotePointSizes(settingsFontFamily, settingsSdFont);
  const uint8_t pt = snapToNearestPointSize(points, textSettings.pointSize);
  std::vector<std::string> labels;
  labels.reserve(points.size());
  int selected = 0;
  for (uint8_t size : points) {
    if (size == pt) selected = static_cast<int>(labels.size());
    char label[40];
    std::snprintf(label, sizeof(label), tr(STR_CARD_POINT_SIZE_FORMAT), unsigned(size));
    labels.emplace_back(label);
  }
  options.show(StrId::STR_SIZE, std::move(labels), selected, [this, points = std::move(points)](int choice) {
    if (choice >= 0 && choice < int(points.size())) {
      textSettings.pointSize = points[choice];
      textSettings.saveFailed = false;
      interactionsReady = false;
    }
  });
}

void QuotesActivity::saveTextSettings() {
  const auto family = SETTINGS.quoteTextFamily;
  const auto pointSize = SETTINGS.quoteTextPointSize;
  const auto alignment = SETTINGS.quoteTextAlignment;
  const bool fontChanged = settingsFontFamily != family || textSettings.pointSize != pointSize ||
                           std::strcmp(settingsSdFont, SETTINGS.quoteSdFontFamily) != 0;
  if (!fontChanged && textSettings.alignment == alignment) {
    back();
    return;
  }
  char previousName[sizeof(settingsSdFont)];
  std::memcpy(previousName, SETTINGS.quoteSdFontFamily, sizeof(previousName));
  SETTINGS.quoteTextFamily = settingsFontFamily;
  SETTINGS.quoteTextPointSize = textSettings.pointSize;
  SETTINGS.quoteTextAlignment = textSettings.alignment;
  std::memcpy(SETTINGS.quoteSdFontFamily, settingsSdFont, sizeof(settingsSdFont));
  if (!SETTINGS.saveToFile()) {
    SETTINGS.quoteTextFamily = family;
    SETTINGS.quoteTextPointSize = pointSize;
    SETTINGS.quoteTextAlignment = alignment;
    std::memcpy(SETTINGS.quoteSdFontFamily, previousName, sizeof(previousName));
    textSettings.saveFailed = true;
    interactionsReady = false;
    requestUpdate();
    LOG_ERR("QUOTES", "Could not save quote text settings");
    return;
  }
  if (fontChanged) {
    quoteFontId = 0;
    quoteGlyphsReady = false;
    paragraph.pages = 0;
  }
  back();
}

void QuotesActivity::route(int action, int value) {
  switch (action) {
    case quotesui::Tab:
      if (value < 0 || value > 2 || (value == tab && !selectedBook)) return;
      tab = value;
      selectedBook = 0;
      bookFavoritesOnly = false;
      listPage = 0;
      oldFirst = false;
      refresh();
      break;
    case quotesui::BookTab:
      if (!selectedBook || value < 0 || value > 1 || bool(value) == bookFavoritesOnly) return;
      bookFavoritesOnly = value == 1;
      listPage = 0;
      refresh();
      break;
    case quotesui::Sort:
      oldFirst = !oldFirst;
      listPage = 0;
      refresh();
      break;
    case quotesui::Open:
      if (value < 0 || value >= visibleCount) return;
      if (tab == 1 && !selectedBook) {
        if (!readQuote(visible[value])) {
          fail();
          return;
        }
        selectedBook = entries[visible[value]].book;
        bookFavoritesOnly = false;
        readingui::copyText(bookTitle, sizeof(bookTitle), quote.title);
        readingui::copyText(bookAuthor, sizeof(bookAuthor), quote.author);
        oldFirst = true;
        listPage = 0;
        refresh();
      } else {
        selection = value;
        bodyPage = 0;
        view = View::Detail;
        refresh(false);
      }
      break;
    case quotesui::Favorite:
      if (view == View::Detail) favorite();
      break;
    case quotesui::Page:
      turn(value);
      break;
    case quotesui::TextPage:
      turnText(value);
      return;
    case quotesui::QuotePage:
      turnQuote(value);
      return;
    case quotesui::TextSettings: {
      if (view != View::List && view != View::Detail) return;
      settingsFontFamily = SETTINGS.quoteTextFamily;
      std::memcpy(settingsSdFont, SETTINGS.quoteSdFontFamily, sizeof(settingsSdFont));
      sdFontSystem.refreshIfDirty();
      if (settingsSdFont[0] && !sdFontSystem.registry().findFamily(settingsSdFont)) settingsSdFont[0] = 0;
      textSettings.pointSize =
          snapToNearestPointSize(quotePointSizes(settingsFontFamily, settingsSdFont), SETTINGS.quoteTextPointSize);
      textSettings.alignment = SETTINGS.quoteTextAlignment;
      textSettings.saveFailed = false;
      textSettingsOrigin = view;
      view = View::TextSettings;
      navigation.clear();
      break;
    }
    case quotesui::PickFont:
      if (view == View::TextSettings) pickFont();
      break;
    case quotesui::PickSize:
      if (view == View::TextSettings) pickSize();
      break;
    case quotesui::SetAlignment:
      if (view != View::TextSettings || value < 0 || value >= pagedtext::kAlignmentCount ||
          value == textSettings.alignment)
        return;
      textSettings.alignment = uint8_t(value);
      break;
    case quotesui::SaveTextSettings:
      if (view != View::TextSettings) return;
      saveTextSettings();
      return;
    case quotesui::Options: {
      if (view != View::Detail) return;
      constexpr StrId actions[] = {StrId::STR_RT_DELETE_QUOTE, StrId::STR_CANCEL};
      options.show(StrId::STR_RT_OPTIONS, actions, 2, 1, [this](int selected) {
        if (selected == 0) view = View::Confirm;
      });
      navigation.clear();
      break;
    }
    case quotesui::ConfirmRemove: {
      if (view != View::Confirm || !visibleCount || !stateWritable) return;
      auto& flags = entries[visible[selection]].flags;
      const auto before = flags;
      flags |= quotes::kHidden;
      if (!quotes::saveFlags(entries, count)) {
        flags = before;
        fail();
        return;
      }
      view = View::List;
      refresh();
      break;
    }
    case quotesui::Cancel:
      if (view == View::Confirm) {
        view = View::Detail;
        refresh();
      } else
        back();
      break;
    case quotesui::Back:
      back();
      return;
  }
  interactionsReady = false;
  requestUpdate();
}
void QuotesActivity::loop() {
  if (view != View::TextSettings && !options.isActive() && !navigation.capture(mappedInput, millis()))
    LOG_ERR("QUOTES", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (options.isActive()) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      options.dismiss();
      interactionsReady = false;
      requestUpdate();
    } else {
      int x, y;
      const bool touchDown = mappedInput.wasScreenTouchDown(x, y);
      options.handleInput(mappedInput, [this, touchDown] {
        // Keep the panel idle between touch-down and release so the render lock can route the tap.
        if (touchDown && options.isActive()) return;
        interactionsReady = false;
        requestUpdate();
      });
    }
    return;
  }
  if (view == View::TextSettings && mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (interactionsReady && surfaceRevealed()) back();
    return;
  }
  if ((view == View::Detail || view == View::TextSettings) && (!interactionsReady || !surfaceRevealed())) return;
  sectionnav::Input::Event command;
  bool navigated = false;
  while (navigation.pop(command, view == View::Detail ? 1 : 32767)) {
    if (command.kind == sectionnav::Input::Kind::Back) {
      navigation.clear();
      back();
      return;
    }
    if (scanning || failed || (view != View::List && view != View::Detail)) continue;
    if (command.kind == sectionnav::Input::Kind::Section) {
      if (view == View::Detail) {
        turnQuote(command.amount);
        return;
      }
      if (selectedBook) {
        if (command.amount % 2) route(quotesui::BookTab, !bookFavoritesOnly);
      } else {
        route(quotesui::Tab, (tab + command.amount % 3 + 3) % 3);
      }
      navigated = true;
    } else if (command.kind == sectionnav::Input::Kind::Step) {
      turn(command.amount);
      if (view == View::Detail) return;
      navigated = true;
    }
  }
  if (navigated) return;
  if (scanning) {
    for (int budget = 0; budget < 4096; ++budget) {
      if (scanAt == scanBytes) {
        if (scanOffset >= scanLength) break;
        const int n = source.read(scanBuffer, std::min(sizeof(scanBuffer), size_t(scanLength - scanOffset)));
        if (n <= 0) {
          scanning = false;
          fail();
          return;
        }
        scanBytes = unsigned(n);
        scanAt = 0;
      }
      const auto c = scanBuffer[scanAt++];
      ++scanOffset;
      scanHash = quotes::hashByte(scanHash, c);
      if (scanHeaderLines < 2 && scanHeaderBytes < sizeof(record) - 1) {
        record[scanHeaderBytes++] = char(c);
        if (c == '\n') ++scanHeaderLines;
      }
      if (scanLine.consume(char(c))) recordQuote();
    }
    if (scanOffset >= scanLength) {
      if (scanLine.consume('\n')) recordQuote();
      scanning = false;
      stateWritable = quotes::loadFlags(entries, count) != quotes::Load::Error;
      refresh();
      if (!stateWritable) fail();
    }
    return;
  }
  const int capacity = listCapacity(toybox::makeTarget(renderer).deviceContext());
  if (view == View::List && cachedCapacity != capacity) {
    if (cachedCapacity > 0) listPage = listPage * cachedCapacity / capacity;
    refresh();
    return;
  }
  const auto swipe = mappedInput.wasSwipe();
  const bool prev = mappedInput.wasReleased(MappedInputManager::Button::Up) ||
                    mappedInput.wasReleased(MappedInputManager::Button::Left) ||
                    swipe == MappedInputManager::SwipeDir::Right;
  const bool next = mappedInput.wasReleased(MappedInputManager::Button::Down) ||
                    mappedInput.wasReleased(MappedInputManager::Button::Right) ||
                    swipe == MappedInputManager::SwipeDir::Left;
  if ((view == View::List || view == View::Detail) && (prev || next)) {
    turn(next ? 1 : -1);
    return;
  }
  int x, y;
  if (!interactionsReady || !surfaceRevealed() || !sidebuttons::poll(interactions, mappedInput, x, y)) return;
  fui::InputSnapshot input{};
  input.touchReleased = true;
  input.touchX = x;
  input.touchY = y;
  const auto action = sidebuttons::route(interactions, input, mappedInput, *this);
  route(action.action, action.value);
}
void QuotesActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  static_assert(readingui::kReadingFont < fui::GfxRendererTarget::FONT_SLOTS);
  if (view == View::Detail && visibleCount && !failed) {
    if (!quoteFontId) updateQuoteFont();
    if (!quoteGlyphsReady) {
      renderer.ensureSdCardFontReady(quoteFontId, quote.body, 0x01);
      quoteGlyphsReady = true;
    }
    target.setFont(readingui::kReadingFont, quoteFontId);
  }
  const int capacity = listCapacity(target.deviceContext());
  if (!scanning && view == View::List && cachedCapacity != capacity) {
    if (cachedCapacity > 0) listPage = listPage * cachedCapacity / capacity;
    refresh(false, false);
  }
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen s(frame);
  if (scanning)
    quotesui::message(s, tr(STR_READING_LOADING));
  else if (view == View::Notice || failed)
    quotesui::message(s, tr(STR_RT_QUOTE_ERROR));
  else if (view == View::Confirm)
    quotesui::message(s, tr(STR_RT_DELETE_CONFIRM), true);
  else if (view == View::TextSettings) {
    textSettings.font = fontLabel(settingsFontFamily, settingsSdFont);
    quotesui::textSettings(s, textSettings);
  } else if (view == View::Detail && visibleCount) {
    quotesui::DetailModel m;
    m.title = quote.title;
    m.author = quote.author;
    m.body = quote.body;
    m.saved = quote.date;
    m.dateFormat = savedDateFormat;
    m.page = quote.page;
    m.bodyPage = bodyPage;
    m.quoteIndex = selection;
    m.quoteCount = visibleCount;
    m.alignment = static_cast<pagedtext::Alignment>(SETTINGS.quoteTextAlignment);
    m.favorite = entries[visible[selection]].flags & quotes::kFavorite;
    bodyPages = quotesui::detail(s, m, paragraph);
    if (bodyPage >= bodyPages) {
      bodyPage = bodyPages - 1;
      requestUpdate();
    }
  } else {
    quotesui::ListModel m;
    m.rows = rows;
    m.visible = rowCount;
    m.total = total;
    m.books = bookCount;
    m.favorites = favoriteCount;
    m.tab = tab;
    m.inBook = selectedBook != 0;
    m.oldest = oldFirst;
    m.bookFavoritesOnly = bookFavoritesOnly;
    m.page = listPage;
    m.pages = std::max(1, (visibleCount + capacity - 1) / capacity);
    m.title = bookTitle;
    m.author = bookAuthor;
    m.bookTotal = bookTotal;
    m.bookFavorites = bookFavorites;
    quotesui::list(s, m);
  }
  options.render(renderer);
  interactionsReady = !scanning && !options.isActive();
  toybox::reportOverflow(interactions, "Quotes", &renderer);
  renderer.displayBuffer();
}
uint32_t QuotesActivity::surfaceMeaning() const {
  return (uint32_t(view) << 28) ^ (uint32_t(options.isActive()) << 27) ^ (uint32_t(bookFavoritesOnly) << 26) ^
         (uint32_t(tab) << 24) ^ (uint32_t(oldFirst) << 23) ^ (uint32_t(bodyPage) << 16) ^ (uint32_t(listPage) << 12) ^
         uint32_t(selection) ^ (uint32_t(selectedBook) ^ uint32_t(selectedBook >> 32));
}
