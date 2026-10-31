#include "FlashHistoryActivity.h"

#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../ui/Dashboard.h"
#include "../ui/PageIndicator.h"
#include "../ui/PixelMenuTheme.h"
#include "CrossPointSettings.h"
#include "FlashSettingsActivity.h"
#include "components/SideButtonNavigation.h"
namespace fui = freeink::ui;
namespace {
void dateLabel(char* out, size_t bytes, int64_t at, bool relative = false) {
  const time_t seconds = at / 1000;
  struct tm t;
  localtime_r(&seconds, &t);
  const time_t now = time(nullptr);
  struct tm today;
  localtime_r(&now, &today);
  const int64_t age = (now - seconds) / 86400;
  if (relative && t.tm_year == today.tm_year && t.tm_yday == today.tm_yday)
    std::snprintf(out, bytes, "%s · %02d:%02d", tr(STR_FLASH_TODAY), t.tm_hour, t.tm_min);
  else if (relative && age >= 0 && age < 2 && t.tm_year == today.tm_year && t.tm_yday == today.tm_yday - 1)
    std::snprintf(out, bytes, "%s · %02d:%02d", tr(STR_FLASH_YESTERDAY), t.tm_hour, t.tm_min);
  else
    std::snprintf(out, bytes, "%02d/%02d/%04d · %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_year + 1900, t.tm_hour,
                  t.tm_min);
}
}  // namespace
std::unique_ptr<Activity> FlashHistoryActivity::create(GfxRenderer& r, MappedInputManager& i, const char* directory,
                                                       Open open) {
  auto app = makeUniqueNoThrow<FlashHistoryActivity>(r, i);
  if (!app) {
    LOG_ERR("FLASH", "OOM: history %u bytes", unsigned(sizeof(FlashHistoryActivity)));
    return nullptr;
  }
  if (directory && std::strlen(directory) >= sizeof(app->onlyDir)) {
    LOG_ERR("FLASH", "History path too long");
    return nullptr;
  }
  if (directory) std::snprintf(app->onlyDir, sizeof(app->onlyDir), "%s", directory);
  app->initial = open;
  return app;
}
void FlashHistoryActivity::addDeck(const char* directory) {
  for (const auto& d : decks)
    if (d.directory == directory) return;
  if (decks.size() == 128) {
    error = true;
    return;
  }
  flashcards::DeckInfo d;
  if (catalog.describe(directory, d, false)) decks.push_back(std::move(d));
}
void FlashHistoryActivity::discover(const char* root) {
  HalFile folder = Storage.open(root);
  if (!folder) return;
  for (auto entry = folder.openNextFile(); entry; entry = folder.openNextFile()) {
    if (!entry.isDirectory()) continue;
    entry.getName(text, sizeof(text));
    if (text[0] == '.') continue;
    const int n = std::snprintf(path, sizeof(path), "%s/%s", root, text);
    if (n < 0 || size_t(n) >= sizeof(path)) {
      error = true;
      continue;
    }
    addDeck(path);
  }
}
void FlashHistoryActivity::discoverAll() {
  if (allDiscovered) return;
  discover("/.crosspoint/flashcards");
  discover(Storage.exists("/FLASHCARDS") ? "/FLASHCARDS" : "/flashcards");
  discover("/study");
  allDiscovered = true;
}
void FlashHistoryActivity::onEnter() {
  Activity::onEnter();
  decks.reserve(128);
  if (onlyDir[0]) {
    addDeck(onlyDir);
    filterIndex = decks.empty() ? -1 : 0;
    error = decks.empty();
  } else {
    discoverAll();
  }
  const time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);
  year = t.tm_year + 1900;
  month = t.tm_mon + 1;
  perPage = flashui::historyCapacity(renderer.getScreenWidth(), renderer.getScreenHeight());
  loadHistory(Direction::Reset);
  if (initial == Open::Cards && !decks.empty()) {
    selected.deck = 0;
    reviewedOnly = false;
    view = View::Cards;
    loadCards();
  } else if (initial == Open::LatestSession && itemCount) {
    selected = items[0];
    view = View::Session;
  }
  requestUpdate();
}
void FlashHistoryActivity::onExit() {
  note.useBuffer(nullptr, 0);
  noteWorkspace.reset();
  Activity::onExit();
}
bool FlashHistoryActivity::newer(const Item& a, const Item& b) {
  if (a.session.started != b.session.started) return a.session.started > b.session.started;
  if (a.deck != b.deck) return a.deck > b.deck;
  return a.session.first > b.session.first;
}
void FlashHistoryActivity::accept(const flashcards::Session& session) {
  const time_t at = session.started / 1000;
  struct tm t;
  localtime_r(&at, &t);
  if (t.tm_year + 1900 != year || t.tm_mon + 1 != month) return;
  ++totalSessions;
  totalCards += session.cards;
  Item candidate{session, scanDeck};
  if (direction == Direction::Older && !newer(anchor, candidate)) return;
  if (direction == Direction::Newer && !newer(candidate, anchor)) return;
  if (itemCount == perPage) {
    if (direction == Direction::Newer) {
      if (!newer(items[0], candidate)) return;
      items[0] = candidate;
    } else {
      if (!newer(candidate, items[perPage - 1])) return;
      items[perPage - 1] = candidate;
    }
  } else
    items[itemCount++] = candidate;
  std::sort(items, items + itemCount, newer);
}
void FlashHistoryActivity::loadHistory(Direction dir) {
  if (itemCount && dir != Direction::Reset) anchor = items[dir == Direction::Older ? itemCount - 1 : 0];
  direction = dir;
  itemCount = totalSessions = totalCards = 0;
  for (scanDeck = 0; scanDeck < int(decks.size()); ++scanDeck) {
    if (filterIndex >= 0 && filterIndex != scanDeck) continue;
    std::snprintf(path, sizeof(path), "%s/revlog.dat", decks[scanDeck].directory.c_str());
    HalFile file;
    if (!Storage.openFileForRead("FLASH", path, file)) continue;
    flashcards::FileSource fileSource(file);
    flashcards::BlockSource source(fileSource, work.readCache, sizeof(work.readCache));
    if (!flashcards::scanHistory(source, work, visit, this, flashcards::kRecentHistoryLimit)) error = true;
    delay(1);
  }
  if (dir == Direction::Reset)
    page = 0;
  else
    page = std::max(0, page + (dir == Direction::Older ? 1 : -1));
  static constexpr StrId months[] = {
      StrId::STR_READING_MONTH_01, StrId::STR_READING_MONTH_02, StrId::STR_READING_MONTH_03,
      StrId::STR_READING_MONTH_04, StrId::STR_READING_MONTH_05, StrId::STR_READING_MONTH_06,
      StrId::STR_READING_MONTH_07, StrId::STR_READING_MONTH_08, StrId::STR_READING_MONTH_09,
      StrId::STR_READING_MONTH_10, StrId::STR_READING_MONTH_11, StrId::STR_READING_MONTH_12};
  std::snprintf(monthLabel, sizeof(monthLabel), "%s %d", I18N.get(months[month - 1]), year);
  for (int n = 0; n < itemCount; ++n) dateLabel(dateLabels[n], sizeof(dateLabels[n]), items[n].session.started, true);
  ready = false;
  requestUpdate();
}
bool FlashHistoryActivity::loadNote(int index) {
  textCache.invalidate();
  if (selected.deck < 0 || selected.deck >= int(decks.size())) return false;
  const auto& dir = decks[selected.deck].directory;
  std::snprintf(path, sizeof(path), "%s/meta.dat", dir.c_str());
  HalFile meta;
  if (!Storage.openFileForRead("FLASH", path, meta)) return false;
  std::snprintf(path, sizeof(path), "%s/deck.dat", dir.c_str());
  HalFile data;
  if (!Storage.openFileForRead("FLASH", path, data)) return false;
  flashcards::FileSource a(meta), b(data);
  if (!noteDeck.openMeta(a) || !noteDeck.openDeck(b)) return false;
  if (noteDeck.hasLongNotes()) {
    // Reused across the card list and detail pages, released on activity exit.
    if (!noteWorkspace) noteWorkspace = HalMemory::allocatePsram(study::kMaxNoteBytes);
    if (!noteWorkspace) {
      LOG_ERR("FLASH", "OOM: history note %u PSRAM bytes", unsigned(study::kMaxNoteBytes));
      return false;
    }
    note.useBuffer(noteWorkspace.get(), study::kMaxNoteBytes);
  } else {
    note.useBuffer(nullptr, 0);
  }
  return noteDeck.loadNote(b, index, note);
}
void FlashHistoryActivity::loadCards() {
  cardCount = 0;
  if (decks.empty()) return;
  const auto& dir = decks[selected.deck].directory;
  std::snprintf(path, sizeof(path), "%s/cards.dat", dir.c_str());
  HalFile states;
  if (!Storage.openFileForRead("FLASH", path, states)) {
    error = true;
    return;
  }
  flashcards::FileSource cardFile(states);
  flashcards::BlockSource cardSource(cardFile, cardReadCache, sizeof(cardReadCache));
  int count = int(states.size() / 32);
  HalFile log;
  flashcards::FileSource logFile(log);
  flashcards::BlockSource logs(logFile, work.readCache, sizeof(work.readCache));
  if (reviewedOnly) {
    std::snprintf(path, sizeof(path), "%s/revlog.dat", dir.c_str());
    if (!Storage.openFileForRead("FLASH", path, log)) {
      error = true;
      return;
    }
    flashcards::Session summary;
    if (!flashcards::summarize(logs, selected.session.first, selected.session.end, work, summary)) {
      error = true;
      return;
    }
    count = summary.cards;
  }
  for (int at = cardPage * 10; at < count && cardCount < 10; ++at) {
    int index = at;
    if (reviewedOnly) {
      int64_t wanted;
      if (!flashcards::reviewedCard(logs, selected.session, at, work, wanted)) {
        error = true;
        break;
      }
      index = -1;
      for (uint32_t p = 0; p + 32 <= cardSource.size(); p += 32) {
        if (!cardSource.read(p, work.record, 32)) {
          error = true;
          break;
        }
        int64_t id;
        std::memcpy(&id, work.record, 8);
        if (id == wanted) {
          index = p / 32;
          break;
        }
      }
    }
    if (index < 0 || !loadNote(index)) {
      error = true;
      continue;
    }
    cardIndices[cardCount] = index;
    const char* question = note.field(note.isCloze() ? study::Field::ClozeQuestion : study::Field::Headword);
    if (std::strlen(question) > 60)
      std::snprintf(cardLabels[cardCount], sizeof(cardLabels[0]), "%s %d", tr(STR_FLASH_QUESTION), index + 1);
    else
      std::snprintf(cardLabels[cardCount], sizeof(cardLabels[0]), "%s", question);
    ++cardCount;
  }
  textPages = std::max(1, (count + 9) / 10);
  ready = false;
  requestUpdate();
}
void FlashHistoryActivity::back() {
  switch (view) {
    case View::History:
      finish();
      return;
    case View::Filter:
      view = View::History;
      loadHistory(Direction::Reset);
      break;
    case View::Session:
      view = View::History;
      break;
    case View::Cards:
      view = reviewedOnly ? View::Session : View::History;
      if (!reviewedOnly && onlyDir[0]) {
        finish();
        return;
      }
      break;
    case View::Card:
      view = View::Cards;
      loadCards();
      break;
  }
  ready = false;
  requestUpdate();
}
void FlashHistoryActivity::loop() {
  RenderLock lock(*this);
  const int capacity = flashui::historyCapacity(renderer.getScreenWidth(), renderer.getScreenHeight());
  if (view == View::History && perPage != capacity) {
    perPage = capacity;
    loadHistory(Direction::Reset);
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    back();
    return;
  }
  int x, y;
  if (!ready) return;
  const int swipe = interactions.routable() ? sidebuttons::swipeStep(mappedInput) : 0;
  const int keyDelta = sidebuttons::shortStep(mappedInput);
  const int delta = swipe ? swipe : keyDelta;
  fui::ActionEvent e;
  if (view == View::History) {
    if (const int direction = sidebuttons::longStep(mappedInput)) {
      e.action = flashui::Month;
      e.value = direction;
    }
  }
  if (keyDelta && view == View::Card) {
    e.action = flashui::ViewSession;
  } else if (!e.action && delta && view != View::Session) {
    e.action = flashui::Page;
    e.value = static_cast<int16_t>(delta);
  } else if (!e.action) {
    const bool held = view == View::Cards && mappedInput.wasScreenLongPress(x, y);
    if (!held && !sidebuttons::poll(interactions, mappedInput, x, y)) return;
    if (held) mappedInput.swallowCurrentTouch();
    fui::InputSnapshot tap;
    tap.longPress = held;
    tap.touchReleased = true;
    tap.touchX = x;
    tap.touchY = y;
    e = sidebuttons::route(interactions, tap, mappedInput, *this);
  }
  if (e.action == flashui::Settings) {
    auto child = FlashSettingsActivity::create(renderer, mappedInput);
    if (child)
      startActivityForResult(std::move(child), [this](const ActivityResult&) {
        textPage = 0;
        textCache.invalidate();
        requestUpdate();
      });
    return;
  }
  if (e.action == flashui::Back) {
    back();
    return;
  }
  if (e.action == flashui::Month) {
    month += e.value;
    if (month < 1) {
      month = 12;
      --year;
    }
    if (month > 12) {
      month = 1;
      ++year;
    }
    loadHistory(Direction::Reset);
    return;
  }
  if (e.action == flashui::Filter) {
    discoverAll();
    view = View::Filter;
    cardPage = 0;
  } else if (e.action == flashui::OpenSession && e.value >= 0 && e.value < itemCount) {
    selected = items[e.value];
    view = View::Session;
  } else if (e.action == flashui::Cards) {
    view = View::Cards;
    reviewedOnly = true;
    cardPage = 0;
    loadCards();
  } else if (e.action == flashui::Page) {
    switch (view) {
      case View::History:
        if ((e.value < 0 && page == 0) || (e.value > 0 && page >= std::max(0, (totalSessions - 1) / perPage))) return;
        loadHistory(e.value > 0 ? Direction::Older : Direction::Newer);
        return;
      case View::Filter:
        cardPage = std::clamp(cardPage + e.value, 0, int(decks.size()) / 4);
        break;
      case View::Cards:
        cardPage = std::clamp(cardPage + e.value, 0, textPages - 1);
        loadCards();
        break;
      case View::Card:
        textPage = std::clamp(textPage + e.value, 0, textPages - 1);
        break;
      case View::Session:
        break;
    }
  } else if (e.action == flashui::OpenCard) {
    if (view == View::Filter) {
      filterIndex = cardPage * 4 + e.value - 1;
      view = View::History;
      loadHistory(Direction::Reset);
      return;
    }
    if (e.value >= 0 && e.value < cardCount && loadNote(cardIndices[e.value])) {
      showAnswer = e.longPress;
      textPage = 0;
      view = View::Card;
    }
  } else if (e.action == flashui::ViewSession) {
    showAnswer = !showAnswer;
    textPage = 0;
  }
  ready = false;
  requestUpdate();
}
void FlashHistoryActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, true);
  target.setFont(4, UI_10_FONT_ID);
  fui::InputSnapshot none;
  toybox::Frame frame(target, target.deviceContext(), none, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  ready = false;
  if (view == View::History) {
    for (int n = 0; n < itemCount; ++n) rows[n] = {decks[items[n].deck].name.c_str(), dateLabels[n], items[n].session};
    flashui::HistoryModel m;
    m.rows = rows;
    m.count = itemCount;
    m.page = page;
    m.pages = std::max(1, (totalSessions + perPage - 1) / perPage);
    m.sessions = totalSessions;
    m.cards = totalCards;
    m.month = monthLabel;
    m.filter = filterIndex >= 0 ? decks[filterIndex].name.c_str() : tr(STR_FLASH_ALL_DECKS);
    m.error = error;
    flashui::history(screen, m);
  } else if (view == View::Session) {
    dateLabel(text, sizeof(text), selected.session.started);
    flashui::SessionModel m;
    m.deck = decks[selected.deck].name.c_str();
    m.date = text;
    m.session = selected.session;
    flashui::session(screen, m);
  } else if (view == View::Cards || view == View::Filter) {
    flashui::CardsModel m;
    m.deck = view == View::Filter ? tr(STR_FLASH_ALL_DECKS) : decks[selected.deck].name.c_str();
    m.page = cardPage;
    m.perPage = view == View::Filter ? 4 : 10;
    m.pages = view == View::Filter ? std::max(1, (int(decks.size()) + 4) / 4) : textPages;
    m.error = error;
    if (view == View::Filter) {
      for (int n = cardPage * 4; n < int(decks.size()) + 1 && m.count < 4; ++n)
        m.labels[m.count++] = n ? decks[n - 1].name.c_str() : tr(STR_FLASH_ALL_DECKS);
    } else {
      m.count = cardCount;
      for (int n = 0; n < cardCount; ++n) m.labels[n] = cardLabels[n];
    }
    flashui::cards(screen, m);
  } else {
    pixelui::begin(screen);
    pixelui::Header h;
    h.back = true;
    h.title = tr(STR_FLASHCARDS_APP);
    flashui::header(screen, h.title);
    backui::publish(true, pixelui::geometry(screen.device()).back);
    auto b = screen.body();
    dashboard::text(screen, fui::makeRect(b.x, b.y, b.width, 40),
                    showAnswer ? tr(STR_FLASH_ANSWER) : tr(STR_FLASH_QUESTION), 1, fui::TextAlign::Left);
    auto face = fui::makeRect(b.x, b.y + 50, b.width, b.height - 174);
    dashboard::frame(screen, face, 2);
    const char* shown =
        note.field(showAnswer ? (note.isCloze() ? study::Field::Sentence : study::Field::Meaning)
                              : (note.isCloze() ? study::Field::ClozeQuestion : study::Field::Headword));
    const auto area = face.inset({12, 12, 12, 12});
    const uint8_t manual =
        flashcards::validTextSize(showAnswer ? SETTINGS.flashcardAnswerSize : SETTINGS.flashcardQuestionSize);
    const int preferences = manual + (SETTINGS.flashcardSmartSize ? 8 : 0);
    if (!textCache.pages || textCache.source != shown || textCache.width != area.width ||
        textCache.height != area.height || preferences != textPreferences) {
      textSize = SETTINGS.flashcardSmartSize ? flashcards::kLargestTextSize : manual;
      for (;;) {
        target.setFont(4, flashcards::textSizeFont(textSize));
        textPages = flashui::textPage(target, area, shown, -1, text, sizeof(text), &textCache, textSize);
        if (!SETTINGS.flashcardSmartSize || (textPages <= 1 && !textCache.wordBroken) ||
            textSize == flashcards::kSmallestTextSize)
          break;
        textSize = flashcards::smallerTextSize(textSize);
      }
      textPreferences = preferences;
    }
    target.setFont(4, flashcards::textSizeFont(textSize));
    textPage = std::clamp(textPage, 0, textPages - 1);
    flashui::textPage(target, area, shown, textPage, text, sizeof(text), &textCache, textSize);
    target.setFont(4, UI_10_FONT_ID);
    pageui::build(screen, fui::makeRect(b.x, b.bottom() - 112, b.width, 48), textPage + 1, textPages, flashui::Page, -1,
                  flashui::Page, 1);
    dashboard::action(screen, fui::makeRect(b.x, b.bottom() - 56, b.width, 56),
                      showAnswer ? tr(STR_FLASH_QUESTION) : tr(STR_FLASH_ANSWER), flashui::ViewSession,
                      &extra_dashboard_eye_32);
  }
  ready = true;
  toybox::reportOverflow(interactions, "Flashcards history", &renderer);
  renderer.displayBuffer();
}
