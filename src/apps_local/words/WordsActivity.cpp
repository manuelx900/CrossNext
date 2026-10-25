#include "WordsActivity.h"

#include <FontCacheManager.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "../Shelf.h"
#include "../flashcards/FlashScreens.h"
#include "../flashcards/FlashSettingsActivity.h"
#include "../reading/ReadingStatsCore.h"
#include "../study/StudySizedDrawTarget.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ToyboxFonts.h"
#include "CrossPointSettings.h"
#include "activities/ActivityManager.h"
#include "activities/reader/EpubReaderActivity.h"
#include "components/HeaderBackTapTarget.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;
namespace {
bool needsReaderFont(const GfxRenderer& renderer, const char* text, int fontId = NOTOSERIF_12_FONT_ID) {
  const auto it = renderer.getFontMap().find(fontId);
  if (it == renderer.getFontMap().end()) return true;
  auto* at = reinterpret_cast<const unsigned char*>(text);
  while (*at) {
    const auto cp = utf8NextCodepoint(&at);
    if (cp >= 0x20 && !it->second.hasCodepoint(cp)) return true;
  }
  return false;
}
}  // namespace
static_assert(sizeof(WordsActivity) <= 32 * 1024, "Keep the vocabulary workspace within its 32 KiB activity budget");

std::unique_ptr<Activity> WordsActivity::create(GfxRenderer& r, MappedInputManager& i) {
  // Fixed page, wire buffer and one loaded word/context: no collection-sized
  // container. Activity lifetime owns this workspace, released on exit.
  auto app = makeUniqueNoThrow<WordsActivity>(r, i);
  if (!app) LOG_ERR("WORDS", "OOM: activity %u bytes", unsigned(sizeof(WordsActivity)));
  return app;
}
void WordsActivity::onEnter() {
  Activity::onEnter();
  resetUi();
  app.setScreen(&WordsActivity::screen, this);
  model.capacity = wordsui::kListCapacity;
  model.filter = filterLabel;
  model.message = message;
  model.text = content;
  vocabulary::copy(filterLabel, sizeof(filterLabel), tr(STR_WORDS_ALL));
  if (!vocabulary::StorageRecords::ready()) {
    failed = true;
    vocabulary::copy(message, sizeof(message), tr(STR_READING_STORAGE_ERROR));
  }
  if (!failed) beginScan();
  requestUpdate();
}
void WordsActivity::onExit() {
  exportJob.cancel();
  directory.close();
  Activity::onExit();
}
void WordsActivity::clock() {
  const auto now = std::time(nullptr);
  std::tm local{};
  model.clockReady = now >= 1577836800 && localtime_r(&now, &local);
  if (!model.clockReady) return;
  today = readingstats::dayIndex({local.tm_year + 1900, local.tm_mon + 1, local.tm_mday});
  if (local.tm_hour < 4) --today;
  minute = ((local.tm_hour + 20) % 24) * 60 + local.tm_min;
}
bool WordsActivity::openContext(const char* id) {
  const auto result = records.context(id, context);
  if (result != vocabulary::Read::Ok) {
    if (result == vocabulary::Read::Error) failed = true;
    return false;
  }
  const auto found = records.word(context.word, word);
  if (found == vocabulary::Read::Error) failed = true;
  return found == vocabulary::Read::Ok;
}
void WordsActivity::beginScan(bool review) {
  directory.close();
  directory = Storage.open(vocabulary::kContexts);
  if (directory && !directory.isDirectory()) directory.close();
  scanned = 0;
  if (wordsui::isList(model.view))
    wordsui::setListCapacity(model,
                             wordsui::listCapacity(model.view, renderer.getScreenWidth(), renderer.getScreenHeight()));
  if (!review) model.count = model.total = model.due = model.eligible = model.difficult = 0;
  if (model.view == wordsui::View::Home) model.saved = model.pending = 0;
  model.busy = scanning = bool(directory);
  scan = review ? Scan::Review : Scan::Collection;
  failed = !directory;
  message[0] = 0;
  if (!directory) vocabulary::copy(message, sizeof(message), tr(STR_READING_STORAGE_ERROR));
  clock();
  resetUi();
  requestUpdate();
}
void WordsActivity::scanStep() {
  for (int budget = 0; budget < 8; ++budget) {
    auto file = directory.openNextFile();
    if (!file) {
      if (!Storage.ready()) failed = true;
      directory.close();
      scanning = model.busy = false;
      if (scan == Scan::Review && !failed) {
        model.reviewTotal = session.count();
        nextCard();
        return;
      }
      if (!failed && wordsui::isList(model.view) && model.total && model.page * model.capacity >= model.total) {
        model.page = (model.total - 1) / model.capacity;
        model.focus = 0;
        beginScan();
        return;
      }
      if (!failed && wordsui::isList(model.view)) {
        const int first = model.view == wordsui::View::List && model.total ? -5 : 0;
        model.focus = std::clamp(model.focus, first, std::max(0, model.count - 1));
        listFocus = model.focus;
      }
      if (failed) vocabulary::copy(message, sizeof(message), tr(STR_READING_STORAGE_ERROR));
      resetUi();
      requestUpdate();
      return;
    }
    if (!vocabulary::recordId(file, vocabulary::kContexts, filename)) continue;
    const bool opened = openContext(filename);
    if (failed) {
      directory.close();
      scanning = model.busy = false;
      vocabulary::copy(message, sizeof(message), tr(STR_READING_STORAGE_ERROR));
      resetUi();
      requestUpdate();
      return;
    }
    if (!opened) continue;
    const bool due = !word.known && model.clockReady && study::Scheduler::isDue(word.card, today, minute);
    if (model.view == wordsui::View::Home) {
      ++model.saved;
      if (due) ++model.pending;
      continue;
    }
    if (context.purpose != purpose) continue;
    if (scan == Scan::Review) {
      if (model.review == wordsui::ReviewKind::AllSaved ||
          (due && (model.review != wordsui::ReviewKind::Difficult || vocabulary::difficult(word))))
        session.add(word, context);
      continue;
    }
    if (scanned >= model.page * model.capacity && model.count < model.capacity) {
      auto& row = model.rows[model.count++];
      vocabulary::copy(row.id, sizeof(row.id), context.id);
      vocabulary::copy(row.label, sizeof(row.label), word.headword);
      vocabulary::copy(row.source, sizeof(row.source), word.meaning);
    }
    ++scanned;
    model.total = scanned;
    if (due) ++model.due;
    if (!word.known) {
      ++model.eligible;
      if (vocabulary::difficult(word) && due) ++model.difficult;
    }
  }
}
void WordsActivity::startCard() {
  model.view = wordsui::View::Review;
  model.answer = false;
  model.focus = 0;
  model.textPage = 0;
  clock();
  buildText();
  preview();
  resetUi();
  requestUpdate();
}
void WordsActivity::preview() {
  clock();
  for (int n = 0; n < 4; ++n) {
    const auto next = scheduler.answer(word.card, static_cast<study::Rating>(n + 1), today, minute);
    flashui::interval(model.delays[n], sizeof(model.delays[n]), next.delayMinutes, next.intervalDays);
  }
}
void WordsActivity::nextCard() {
  clock();
  currentCard = session.next(today, minute);
  if (currentCard >= 0) {
    if (!openContext(session.entry(currentCard).context)) {
      failed = true;
      vocabulary::copy(message, sizeof(message), tr(STR_READING_STORAGE_ERROR));
      resetUi();
      requestUpdate();
      return;
    }
    startCard();
    return;
  }
  model.waitMinutes = session.waitingMinutes(today, minute);
  if (model.waitMinutes >= 0)
    model.view = wordsui::View::Waiting;
  else if (model.review == wordsui::ReviewKind::AllSaved && session.nextBatch()) {
    beginScan(true);
    return;
  } else {
    model.view = listReturn;
    beginScan();
    vocabulary::copy(message, sizeof(message),
                     model.reviewIndex ? tr(STR_WORDS_SESSION_DONE) : tr(STR_WORDS_CAUGHT_UP));
  }
  resetUi();
  requestUpdate();
}
void WordsActivity::buildText() {
  textLayoutValid = false;
  prewarmText = true;
  model.textCache.invalidate();
  model.known = word.known;
  std::snprintf(content, sizeof(content), "%s%s", word.meaning, word.shortened ? tr(STR_WORDS_SHORTENED) : "");
  model.meaning = content;
  const auto sentenceAt = std::strlen(content) + 1;
  std::snprintf(content + sentenceAt, sizeof(content) - sentenceAt, "%s%s",
                context.fragment ? tr(STR_WORDS_FRAGMENT) : "", context.sentence);
  model.sentence = content + sentenceAt;
  model.readerContent = needsReaderFont(renderer, word.headword) || needsReaderFont(renderer, word.meaning) ||
                        needsReaderFont(renderer, context.sentence) || needsReaderFont(renderer, context.book);
  if (!model.readerContent) return;
  const int font = SETTINGS.getReaderFontId();
  renderer.ensureSdCardFontReady(font, model.meaning, 0x07);
  renderer.ensureSdCardFontReady(font, model.sentence, 0x07);
  renderer.ensureSdCardFontReady(font, word.headword, 0x07);
  renderer.ensureSdCardFontReady(font, context.book, 0x07);
}
void WordsActivity::back() {
  if (model.view == wordsui::View::Home) {
    shelf::leave(renderer, mappedInput);
    return;
  }
  directory.close();
  if (model.view == wordsui::View::Guide) {
    model.view = guideReturn;
    model.focus = 0;
    beginScan();
    return;
  }
  if (model.view == wordsui::View::Delete) {
    model.view = wordsui::View::Detail;
    model.focus = 1;
    resetUi();
    requestUpdate();
    return;
  }
  if (model.view == wordsui::View::WordList) {
    model.view = listReturn = wordsui::View::List;
    model.page = collectionPage;
    model.focus = listFocus = collectionFocus;
    model.capacity = wordsui::kListCapacity;
    beginScan();
    return;
  }
  if (model.view != wordsui::View::List) {
    model.view = listReturn;
    model.focus = listFocus;
    model.textPage = 0;
    beginScan();
  } else {
    model.view = wordsui::View::Home;
    model.focus = 0;
    beginScan();
  }
}
void WordsActivity::dispatch(int action, int value) {
  using namespace wordsui;
  if ((scanning || exporting) && action != Back) return;
  switch (action) {
    case Back:
      back();
      return;
    case Help:
      guideReturn = model.view;
      model.view = View::Guide;
      model.guidePage = 0;
      break;
    case wordsui::Settings: {
      auto child = FlashSettingsActivity::create(renderer, mappedInput, flashcards::TextApp::Words);
      if (child)
        startActivityForResult(std::move(child), [this](const ActivityResult&) {
          textLayoutValid = false;
          prewarmText = true;
          model.textPage = 0;
          model.textCache.invalidate();
          resetUi();
          requestUpdate();
        });
      return;
    }
    case Dismiss:
      message[0] = '\0';
      if (isList(model.view)) {
        model.page = model.focus = 0;
        beginScan();
      }
      break;
    case Vocabulary:
    case Languages:
      purpose = action == Languages ? vocabulary::Purpose::Language : vocabulary::Purpose::Vocabulary;
      model.view = listReturn = View::List;
      model.page = model.focus = 0;
      vocabulary::copy(filterLabel, sizeof(filterLabel),
                       action == Languages ? tr(STR_WORDS_TRANSLATIONS) : tr(STR_WORDS_COLLECTION));
      beginScan();
      break;
    case FullList:
      collectionPage = model.page;
      collectionFocus = model.focus;
      model.view = listReturn = View::WordList;
      model.page = model.focus = 0;
      beginScan();
      break;
    case Open:
      if (value < 0 || value >= model.count) return;
      if (!openContext(model.rows[value].id)) {
        if (failed) {
          resetUi();
          requestUpdate();
        }
        return;
      }
      listFocus = value;
      listReturn = model.view;
      model.view = View::Detail;
      model.textPage = model.focus = 0;
      buildText();
      break;
    case Previous:
    case Next:
      if (model.view == View::Guide) {
        model.guidePage = std::clamp(model.guidePage + (action == Next ? 1 : -1), 0, kGuidePages - 1);
      } else if (isList(model.view)) {
        const int next = model.page + (action == Next ? 1 : -1);
        if (next < 0 || next * model.capacity >= model.total) return;
        model.page = next;
        model.focus = 0;
        beginScan();
      } else
        model.textPage = std::clamp(model.textPage + (action == Next ? 1 : -1), 0, std::max(0, model.textPages - 1));
      break;
    case Review:
    case ReviewHard:
    case ReviewAll:
    case ReviewWord:
      clock();
      if (!model.clockReady) {
        vocabulary::copy(message, sizeof(message), tr(STR_WORDS_CLOCK));
        failed = model.view == View::Detail;
        resetUi();
        requestUpdate();
        return;
      }
      switch (action) {
        case ReviewHard:
          model.review = ReviewKind::Difficult;
          break;
        case ReviewAll:
          model.review = ReviewKind::AllSaved;
          break;
        case ReviewWord:
          model.review = ReviewKind::SingleWord;
          break;
        default:
          model.review = ReviewKind::DueWords;
          break;
      }
      model.reviewIndex = 0;
      session.clear(action == ReviewAll    ? vocabulary::ReviewSession::Selection::AllSaved
                    : action == ReviewHard ? vocabulary::ReviewSession::Selection::Difficult
                                           : vocabulary::ReviewSession::Selection::Due);
      if (action == ReviewWord) {
        session.add(word, context);
        model.reviewTotal = 1;
        currentCard = 0;
        startCard();
      } else
        beginScan(true);
      break;
    case Reveal:
      if (model.view != View::Review || !model.clockReady) return;
      model.answer = true;
      model.textPage = 0;
      buildText();
      preview();
      break;
    case Rate:
      if (model.view != View::Review || !model.answer || value < 1 || value > 4) return;
      clock();
      if (!model.clockReady) return;
      {
        // Re-read before writing so neither another encounter nor a recovered
        // record can be replaced by stale in-memory scheduling state.
        if (records.word(context.word, word) != vocabulary::Read::Ok) {
          failed = true;
          break;
        }
        vocabulary::rate(word, static_cast<study::Rating>(value), scheduler, today, minute);
        if (!records.put(word)) {
          failed = true;
          vocabulary::copy(message, sizeof(message), tr(STR_READING_STORAGE_ERROR));
          break;
        }
        ++model.reviewIndex;
        session.answered(currentCard, word.card);
        nextCard();
      }
      break;
    case Delete:
      model.view = View::Delete;
      model.focus = 1;
      break;
    case Cancel:
      model.view = View::Detail;
      model.focus = 1;
      break;
    case ConfirmDelete:
      if (model.view != View::Delete) return;
      if (!records.removeContext(context.id)) {
        failed = true;
        vocabulary::copy(message, sizeof(message), tr(STR_READING_STORAGE_ERROR));
        break;
      }
      model.view = listReturn;
      model.focus = listFocus = 0;
      model.page = 0;
      beginScan();
      break;
    case Passage:
      if (Storage.exists(context.path)) {
        auto reader = EpubReaderActivity::openVocabulary(renderer, mappedInput, context);
        if (reader) {
          directory.close();
          activityManager.replaceActivity(std::move(reader));
          return;
        }
      }
      vocabulary::copy(message, sizeof(message), tr(STR_WORDS_MISSING_BOOK));
      failed = true;
      break;
    case Export:
      // Run after the pending screen has actually painted, outside RenderLock.
      exporting = true;
      model.busy = true;
      vocabulary::copy(message, sizeof(message), tr(STR_WORDS_EXPORTING));
      break;
    default:
      return;
  }
  resetUi();
  requestUpdate();
}
void WordsActivity::screen(UiScreen& screen, void* user) {
  auto& self = *static_cast<WordsActivity*>(user);
  pixelui::begin(screen);
  pixelui::bindFonts(self.uiTarget, self.renderer, true);
  if (wordsui::isList(self.model.view)) self.uiTarget.setFont(1, toybox::kProseFontId);
  const bool detail = self.model.view == wordsui::View::Detail;
  const bool review = self.model.view == wordsui::View::Review;
  self.model.term = self.word.headword;
  self.model.book = self.context.book;
  const uint8_t manualTitle = flashcards::validTextSize(SETTINGS.wordsQuestionSize);
  const uint8_t manualBody = flashcards::validTextSize(SETTINGS.wordsAnswerSize);
  const bool smart = SETTINGS.wordsSmartSize;
  const int preferences = manualTitle + 5 * manualBody + (smart ? 32 : 0);
  study::SizedDrawTarget contentTarget(self.renderer, self.uiTarget);
  auto bind = [&] {
    self.uiTarget.setFont(
        2, self.model.readerContent ? SETTINGS.getReaderFontId() : flashcards::textSizeFont(self.titleSize));
    self.uiTarget.setFont(
        4, self.model.readerContent ? SETTINGS.getReaderFontId() : flashcards::textSizeFont(self.bodySize));
    contentTarget.bind(self.model.readerContent ? SETTINGS.getReaderFontId() : flashcards::textSizeFont(self.titleSize),
                       self.model.readerContent ? SETTINGS.getReaderFontId() : flashcards::textSizeFont(self.bodySize),
                       self.titleSize, self.bodySize);
  };
  if (detail || review) {
    const auto area = wordsui::textBox(self.uiTarget.deviceContext(), self.model);
    if (!self.textLayoutValid || self.layoutAnswer != self.model.answer || self.layoutDetail != detail ||
        self.layoutWidth != area.width || self.layoutHeight != area.height || self.layoutPreferences != preferences) {
      self.model.textCache.invalidate();
      self.bodySize = smart ? flashcards::kLargestTextSize : manualBody;
      self.titleSize = smart ? flashcards::kLargestTextSize : manualTitle;
      for (;;) {
        bind();
        uint8_t wordBreaks = 0;
        const int pages = wordsui::measureCard(contentTarget, area, self.model, self.wrap, nullptr, &wordBreaks, true);
        if (!smart) break;
        if ((wordBreaks & 1) && self.titleSize != flashcards::kSmallestTextSize) {
          self.titleSize = flashcards::smallerTextSize(self.titleSize);
          continue;
        }
        if (((wordBreaks & 2) || (pages > 1 && (detail || self.model.answer))) &&
            self.bodySize != flashcards::kSmallestTextSize &&
            (self.bodySize != flashcards::kSmartTextFloor || (wordBreaks & 2))) {
          self.bodySize = flashcards::smallerTextSize(self.bodySize);
          continue;
        }
        if (pages > 1 && ((review && !self.model.answer) || self.titleSize > 1) &&
            self.titleSize != flashcards::kSmartTextFloor && self.titleSize != flashcards::kSmallestTextSize) {
          self.titleSize = flashcards::smallerTextSize(self.titleSize);
          continue;
        }
        break;
      }
      if (smart) {
        uint8_t wordBreaks = 0;
        wordsui::measureCard(contentTarget, area, self.model, self.wrap, nullptr, &wordBreaks);
        const bool smallerTitle = (wordBreaks & 1) && self.titleSize != flashcards::kSmallestTextSize;
        const bool smallerBody = (wordBreaks & 2) && self.bodySize != flashcards::kSmallestTextSize;
        if (smallerTitle || smallerBody) {
          if (smallerTitle) self.titleSize = flashcards::kSmallestTextSize;
          if (smallerBody) self.bodySize = flashcards::kSmallestTextSize;
          self.model.textCache.invalidate();
          bind();
        }
      }
      self.textLayoutValid = true;
      self.layoutAnswer = self.model.answer;
      self.layoutDetail = detail;
      self.layoutWidth = area.width;
      self.layoutHeight = area.height;
      self.layoutPreferences = preferences;
    }
    bind();
  }
  if (review || wordsui::isList(self.model.view)) self.model.keyboardFocus = false;
  self.model.error = self.failed;
  self.model.term = self.word.headword;
  self.model.book = self.context.book;
  self.model.language = self.word.language;
  self.model.translation = self.context.purpose == vocabulary::Purpose::Language;
  wordsui::build(screen, self.model, self.geometry, self.wrap, &contentTarget);
}
void WordsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  pixelui::drawHeader(renderer, tr(STR_WORDS_APP), true);
  backui::publish(true, pixelui::geometry(uiTarget.deviceContext()).back);
  if (prewarmText && model.readerContent) {
    auto* cache = renderer.getFontCacheManager();
    auto warm = cache->createPrewarmScope();
    renderUi();
    warm.endScanAndPrewarm();
  }
  renderUi();
  prewarmText = false;
  renderer.displayBuffer();
}
void WordsActivity::loop() {
  if (exporting) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      exportJob.cancel();
      RenderLock lock;
      exporting = model.busy = false;
      vocabulary::copy(message, sizeof(message), tr(STR_WORDS_EXPORT_CANCELLED));
      resetUi();
      requestUpdate();
      return;
    }
    if (!routingReady()) return;
    if (exportJob.idle()) exportJob.begin(purpose, "", -1, "", model.total);
    if (!exportJob.finished()) exportJob.step();
    if (exportJob.finished()) {
      const bool ok = exportJob.succeeded();
      exportJob.cancel();
      RenderLock lock;
      exporting = model.busy = false;
      failed = !ok;
      vocabulary::copy(message, sizeof(message), ok ? tr(STR_WORDS_EXPORTED) : tr(STR_READING_STORAGE_ERROR));
      resetUi();
      requestUpdate();
    }
    return;
  }
  RenderLock lock;
  if (wordsui::isList(model.view)) {
    const int capacity = wordsui::listCapacity(model.view, renderer.getScreenWidth(), renderer.getScreenHeight());
    if (capacity != model.capacity) {
      beginScan();
    }
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    failed = false;
    back();
    return;
  }
  if (failed) {
    const auto result = routeTouch(mappedInput, false, false, true);
    if (result.routed && app.invalidated()) requestUpdate();
    if ((result && result.event.action == wordsui::Back) ||
        mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
      failed = false;
      back();
    }
    return;
  }
  if (model.view == wordsui::View::Waiting && millis() - lastPoll >= 1000) {
    lastPoll = millis();
    const int previous = model.waitMinutes;
    clock();
    if (session.next(today, minute) >= 0) {
      nextCard();
      return;
    }
    model.waitMinutes = session.waitingMinutes(today, minute);
    if (model.waitMinutes != previous) requestUpdate();
  }
  if (scanning) {
    scanStep();
    return;
  }
  if (routingReady()) {
    if (const int direction = sidebuttons::swipeStep(mappedInput)) {
      switch (model.view) {
        case wordsui::View::List:
        case wordsui::View::WordList:
        case wordsui::View::Guide:
          dispatch(direction > 0 ? wordsui::Next : wordsui::Previous);
          return;
        case wordsui::View::Detail:
        case wordsui::View::Review:
          if (model.textPages > 1) dispatch(direction > 0 ? wordsui::Next : wordsui::Previous);
          return;
        case wordsui::View::Home:
        case wordsui::View::Waiting:
        case wordsui::View::Delete:
          break;
      }
    }
    if (const int direction = sidebuttons::longStep(mappedInput)) {
      if (model.view == wordsui::View::List)
        dispatch(wordsui::FullList);
      else if (model.view == wordsui::View::Review && !model.answer)
        dispatch(wordsui::Reveal);
      else
        dispatch(direction > 0 ? wordsui::Next : wordsui::Previous);
      return;
    }
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      if (model.view == wordsui::View::Review) {
        model.keyboardFocus = false;
        dispatch(!model.answer ? wordsui::Reveal : direction > 0 ? wordsui::Next : wordsui::Previous);
        return;
      }
      if (model.view == wordsui::View::List) {
        model.keyboardFocus = false;
        dispatch(direction > 0 ? wordsui::Next : wordsui::Previous);
        return;
      }
      if (model.view == wordsui::View::Guide || model.view == wordsui::View::WordList) {
        dispatch(direction > 0 ? wordsui::Next : wordsui::Previous);
        return;
      }
      int first = 0, last = 0;
      if (model.view == wordsui::View::Home)
        last = 3;
      else if (model.view == wordsui::View::Detail)
        last = 2;
      else if (model.view == wordsui::View::Delete)
        last = 1;
      model.focus = model.keyboardFocus
                        ? first + (model.focus - first + direction + last - first + 1) % (last - first + 1)
                        : first;
      model.keyboardFocus = true;
      requestUpdate();
      return;
    }
  }
  const auto routed = routeTouch(mappedInput, false, false, false);
  if (routed.routed && app.invalidated()) requestUpdate();
  if (routed) {
    model.keyboardFocus = false;
    app.clearTapFlash();
    dispatch(routed.event.action, routed.event.value);
    return;
  }
  if (routed.routed) return;
  if (model.view == wordsui::View::Review || wordsui::isList(model.view)) return;
  if (mappedInput.wasReleased(MappedInputManager::Button::Left))
    dispatch(wordsui::Previous);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Right))
    dispatch(wordsui::Next);
  else if (mappedInput.wasReleased(MappedInputManager::Button::Up) ||
           mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    const int count = model.view == wordsui::View::Home                          ? 4
                      : wordsui::isList(model.view) && !model.total && !*message ? 2
                      : model.view == wordsui::View::Review && model.answer      ? 4
                      : model.view == wordsui::View::Detail                      ? 3
                      : model.view == wordsui::View::Delete                      ? 2
                                                                                 : model.count;
    const int first = model.view == wordsui::View::List && model.total ? -5 : 0;
    model.keyboardFocus = true;
    model.focus = std::clamp(model.focus + (mappedInput.wasReleased(MappedInputManager::Button::Up) ? -1 : 1), first,
                             std::max(0, count - 1));
    requestUpdate();
  } else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (model.view == wordsui::View::Home)
      dispatch(model.focus == 0   ? wordsui::Vocabulary
               : model.focus == 1 ? wordsui::Languages
               : model.focus == 2 ? wordsui::Help
                                  : wordsui::Settings);
    else if (wordsui::isList(model.view) && *message)
      dispatch(wordsui::Dismiss);
    else if (wordsui::isList(model.view) && !model.total)
      dispatch(model.focus == 0 ? wordsui::Help : wordsui::Back);
    else if (model.view == wordsui::View::List && model.focus < 0) {
      static constexpr int buttons[] = {wordsui::Review, wordsui::ReviewHard, wordsui::ReviewAll, wordsui::Export,
                                        wordsui::FullList};
      dispatch(buttons[model.focus + 5]);
    } else if (wordsui::isList(model.view) && model.count)
      dispatch(wordsui::Open, model.focus);
    else if (model.view == wordsui::View::Detail) {
      static constexpr int buttons[] = {wordsui::Passage, wordsui::Delete, wordsui::ReviewWord};
      dispatch(buttons[model.focus]);
    } else if (model.view == wordsui::View::Delete) {
      dispatch(model.focus == 0 ? wordsui::ConfirmDelete : wordsui::Cancel);
    } else if (model.view == wordsui::View::Waiting) {
      dispatch(wordsui::Back);
    } else if (model.view == wordsui::View::Review && !model.answer)
      dispatch(wordsui::Reveal);
  }
}
void WordsActivity::prepareForSleep() {
  if (!exporting) return;
  exportJob.cancel();
  exporting = model.busy = false;
  vocabulary::copy(message, sizeof(message), tr(STR_WORDS_EXPORT_CANCELLED));
}
