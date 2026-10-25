#include "QuotesActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ReadingToolsUi.h"
#include "../ui/ToyboxFonts.h"
#include "QuotesStore.h"
#include "components/SideButtonNavigation.h"
namespace fui = freeink::ui;
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
  if (Storage.exists("/My Clippings.txt")) {
    scanning = Storage.openFileForRead("QUOTES", "/My Clippings.txt", source);
    failed = !scanning;
    if (scanning && source.fileSize64() > UINT32_MAX) {
      scanning = false;
      failed = true;
    }
  }
  requestUpdate();
}
void QuotesActivity::onExit() {
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
  if (readQuote(slot)) {
    entry.book = quotes::hashByte(quotes::hash(quote.title), 0);
    for (const char* p = quote.author; *p; ++p) entry.book = quotes::hashByte(entry.book, uint8_t(*p));
    entry.page = std::clamp(quote.page, 0, 65535);
  } else
    failed = true;
  nextStart = end;
  scanHash = quotes::kHashStart;
}
void QuotesActivity::rebuild() {
  visibleCount = bookCount = favoriteCount = total = bookTotal = bookFavorites = 0;
  for (int rank = 0; rank < count; ++rank) {
    const int slot = (oldest + count - 1 - rank) % quotes::kMax;
    const auto& e = entries[slot];
    if (e.flags & quotes::kHidden) continue;
    ++total;
    if (e.flags & quotes::kFavorite) ++favoriteCount;
    bool found = false;
    for (int n = 0; n < bookCount; ++n)
      if (entries[groups[n]].book == e.book) found = true;
    if (!found) groups[bookCount++] = slot;
    if (selectedBook && e.book == selectedBook) {
      ++bookTotal;
      if (e.flags & quotes::kFavorite) ++bookFavorites;
    }
    if (selectedBook ? e.book == selectedBook : tab == 2 ? bool(e.flags & quotes::kFavorite) : tab == 0)
      visible[visibleCount++] = slot;
  }
  if (tab == 1 && !selectedBook) {
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
  cachedCapacity = quotesui::capacity(toybox::makeTarget(renderer).deviceContext());
  if (!visibleCount && (view == View::Detail || view == View::Confirm)) view = View::List;
  if (view == View::Detail || view == View::Confirm) {
    if (visibleCount && !readQuote(visible[selection])) fail();
    return;
  }
  const int cap = quotesui::capacity(toybox::makeTarget(renderer).deviceContext());
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
    r.favorite = entries[slot].flags & quotes::kFavorite;
    for (int k = 0; tab == 1 && k < count; ++k)
      if (!(entries[k].flags & quotes::kHidden) && entries[k].book == entries[slot].book) {
        ++r.count;
        if (entries[k].flags & quotes::kFavorite) ++r.favorites;
      }
  }
  interactionsReady = false;
  if (repaint) requestUpdate();
}
void QuotesActivity::favorite(int n) {
  if (n < 0 || n >= visibleCount || !stateWritable) {
    fail();
    return;
  }
  auto& flags = entries[visible[n]].flags;
  flags ^= quotes::kFavorite;
  if (!quotes::saveFlags(entries, count)) {
    flags ^= quotes::kFavorite;
    fail();
    return;
  }
  bodyPage = 0;
  refresh();
  interactionsReady = false;
  requestUpdate();
}
void QuotesActivity::turn(int step) {
  if (view == View::Detail) {
    if (bodyPage + step >= 0 && bodyPage + step < bodyPages)
      bodyPage += step;
    else if (selection + step >= 0 && selection + step < visibleCount) {
      selection += step;
      bodyPage = 0;
      refresh();
    }
  } else if (view == View::List) {
    const int pages = std::max(1, (visibleCount + std::max(1, cachedCapacity) - 1) / std::max(1, cachedCapacity));
    const int next = std::clamp(listPage + step, 0, pages - 1);
    if (next == listPage) return;
    listPage = next;
    refresh(false);
  }
  interactionsReady = false;
  requestUpdate();
}
void QuotesActivity::back() {
  if (view == View::Notice) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  if (view == View::Confirm) {
    view = View::Detail;
    requestUpdate();
    return;
  }
  if (view == View::Detail) {
    view = View::List;
    listPage = selection / quotesui::capacity(toybox::makeTarget(renderer).deviceContext());
    refresh();
    return;
  }
  if (selectedBook) {
    selectedBook = 0;
    oldFirst = false;
    listPage = 0;
    refresh();
    return;
  }
  shelf::leave(renderer, mappedInput);
}
void QuotesActivity::route(int action, int value) {
  switch (action) {
    case quotesui::Tab:
      if (value < 0 || value > 2 || (value == tab && !selectedBook)) return;
      tab = value;
      selectedBook = 0;
      listPage = 0;
      oldFirst = false;
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
        readingui::copyText(bookTitle, sizeof(bookTitle), quote.title);
        readingui::copyText(bookAuthor, sizeof(bookAuthor), quote.author);
        oldFirst = true;
        listPage = 0;
        refresh();
      } else {
        selection = value;
        bodyPage = 0;
        view = View::Detail;
        refresh();
      }
      break;
    case quotesui::Favorite:
      favorite(view == View::Detail ? selection : value);
      break;
    case quotesui::Page:
      turn(value);
      break;
    case quotesui::Remove:
      if (view != View::Detail) return;
      view = View::Confirm;
      break;
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
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("QUOTES", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  if (view == View::Detail && (!interactionsReady || !surfaceRevealed())) return;
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
      route(quotesui::Tab, (tab + command.amount % 3 + 3) % 3);
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
        if (scanOffset >= source.fileSize64()) break;
        if (!source.seekSet(scanOffset)) {
          scanning = false;
          fail();
          return;
        }
        const int n = source.read(scanBuffer, sizeof(scanBuffer));
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
      if (scanLine.consume(char(c))) recordQuote();
    }
    if (scanOffset >= source.fileSize64()) {
      if (scanLine.consume('\n')) recordQuote();
      scanning = false;
      stateWritable = quotes::loadFlags(entries, count) != quotes::Load::Error;
      refresh();
      if (!stateWritable) fail();
    }
    return;
  }
  if (view == View::List && cachedCapacity != quotesui::capacity(toybox::makeTarget(renderer).deviceContext())) {
    if (cachedCapacity > 0)
      listPage = listPage * cachedCapacity / quotesui::capacity(toybox::makeTarget(renderer).deviceContext());
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
  target.setFont(readingui::kReadingFont, toybox::kSerifTitleFontId);
  if (!scanning && view == View::List && cachedCapacity != quotesui::capacity(target.deviceContext())) {
    if (cachedCapacity > 0) listPage = listPage * cachedCapacity / quotesui::capacity(target.deviceContext());
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
  else if (view == View::Detail && visibleCount) {
    quotesui::DetailModel m;
    m.title = quote.title;
    m.author = quote.author;
    m.body = quote.body;
    m.saved = quote.date;
    m.page = quote.page;
    m.index = selection;
    m.total = visibleCount;
    m.bodyPage = bodyPage;
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
    m.page = listPage;
    m.pages = std::max(1, (visibleCount + quotesui::capacity(target.deviceContext()) - 1) /
                              quotesui::capacity(target.deviceContext()));
    m.title = bookTitle;
    m.author = bookAuthor;
    m.bookTotal = bookTotal;
    m.bookFavorites = bookFavorites;
    quotesui::list(s, m);
  }
  interactionsReady = !scanning;
  toybox::reportOverflow(interactions, "Quotes", &renderer);
  renderer.displayBuffer();
}
uint32_t QuotesActivity::surfaceMeaning() const {
  return (uint32_t(view) << 28) ^ (uint32_t(tab) << 24) ^ (uint32_t(oldFirst) << 23) ^ (uint32_t(bodyPage) << 16) ^
         (uint32_t(listPage) << 12) ^ uint32_t(selection) ^ (uint32_t(selectedBook) ^ uint32_t(selectedBook >> 32));
}
