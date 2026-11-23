#include "DictionaryWordSelectActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <Memory.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "CrossPointSettings.h"
#include "DictionaryDefinitionActivity.h"
#include "components/UITheme.h"

namespace {

constexpr unsigned long POPUP_DURATION_MS = 1500;

void indexBuildYield(void*) { vTaskDelay(1); }

}  // namespace

void DictionaryWordSelectActivity::onEnter() {
  Activity::onEnter();
  fontId = SETTINGS.getReaderFontId();
  if (!dictionaryOverride)
    std::snprintf(dictionaryName, sizeof(dictionaryName), "%s",
                  translate ? SETTINGS.translationDictionaryName : SETTINGS.dictionaryName);
  lookupPending = true;
}

void DictionaryWordSelectActivity::performLookup() {
  const auto showIndexing = [](void* context) {
    auto& self = *static_cast<DictionaryWordSelectActivity*>(context);
    {
      RenderLock lock;
      if (self.popup == Popup::Busy) return;
      self.popupMsg = StrId::STR_DICT_INDEXING;
      self.popup = Popup::Busy;
    }
    self.requestUpdate(true);
  };
  if (!dictOpenAttempted) {
    dictOpenAttempted = true;
    dictOpenOk = dict.open(dictionaryName);
    // needsIndex() opens and validates the .qidx sidecar, so ask it once per
    // open rather than once per word: the answer only changes when we build
    // the sidecar ourselves, which is handled below.
    dictNeedsIndex = dictOpenOk && dict.needsIndex();
  }
  bool ok = dictOpenOk;
  Dictionary::IndexResult indexResult = Dictionary::IndexResult::Ok;
  if (ok && dictNeedsIndex) {
    showIndexing(this);
    ok = dict.buildIndex(&indexBuildYield, nullptr, &indexResult);
    dictNeedsIndex = !ok;  // a successful build leaves the sidecar fresh; a failed one retries
  }

  std::string definition;
  std::string headword;
  Dictionary::LookupResult result = Dictionary::LookupResult::NotFound;
  const bool found = ok && dict.lookup(lookupText.c_str(), definition, headword, &result, showIndexing, this);

  if (found) {
    vocabulary::CaptureNotice notice;
    if (captureRequested && !source) {
      notice.attempted = true;
      notice.status = vocabulary::Capture::Failed;
    }
    auto activity = makeUniqueNoThrow<DictionaryDefinitionActivity>(
        renderer, mappedInput, std::move(headword), std::move(definition), dict.definitionsAreHtml(), notice);
    if (!activity) {
      LOG_ERR("DICT", "OOM: definition activity");
      RenderLock lock;
      popup = Popup::Error;
      popupMsg = StrId::STR_DICT_LOW_MEMORY;
      popupTime = millis();
      requestUpdate();
      return;
    }
    {
      RenderLock lock;
      popup = Popup::None;
      activity->showOverPage(std::move(page), fontId, marginLeft, marginTop, selection);
    }
    activity->enableLookup(dict, lookupText);
    if (source) {
      source->purpose = translate ? vocabulary::Purpose::Language : vocabulary::Purpose::Vocabulary;
      if (!vocabulary::copy(source->surface, sizeof(source->surface), lookupText.c_str())) source->surface[0] = 0;
      activity->offerVocabulary(std::move(source), dictionaryName, sourceLanguage.c_str());
    }
    startActivityForResult(std::move(activity), [this](const ActivityResult&) { finish(); });
    return;
  }
  // Name the failure: a genuine miss is "Not found"; a word that WAS found but
  // couldn't be read is a real error — and we distinguish decompression from a
  // low-memory allocation from a generic read error.
  RenderLock lock;
  if (!ok) {
    popup = Popup::Error;
    // An index build allocates a scan buffer, so it fails the same way lookups
    // do on a fragmented heap — name that rather than a generic error.
    switch (indexResult) {
      case Dictionary::IndexResult::LowMemory:
        popupMsg = StrId::STR_DICT_LOW_MEMORY;
        break;
      case Dictionary::IndexResult::ReadError:
        popupMsg = StrId::STR_DICT_READ_FAILED;
        break;
      case Dictionary::IndexResult::Ok:
      default:
        popupMsg = translate ? StrId::STR_WORD_TRANSLATION_SETUP : StrId::STR_WORD_DEFINITION_SETUP;
        break;
    }
  } else {
    switch (result) {
      case Dictionary::LookupResult::Decompress:
        popup = Popup::Error;
        popupMsg = StrId::STR_DICT_DECOMPRESS_ERROR;
        break;
      case Dictionary::LookupResult::LowMemory:
        popup = Popup::Error;
        popupMsg = StrId::STR_DICT_LOW_MEMORY;
        break;
      case Dictionary::LookupResult::ReadError:
        popup = Popup::Error;
        popupMsg = StrId::STR_DICT_READ_FAILED;
        break;
      case Dictionary::LookupResult::NotFound:
      default:
        popup = Popup::NotFound;
        popupMsg = StrId::STR_DICT_NOT_FOUND;
        break;
    }
  }
  popupTime = millis();
  requestUpdate();
}

void DictionaryWordSelectActivity::loop() {
  if (lookupPending) {
    lookupPending = false;
    performLookup();
    return;
  }
  if (popup == Popup::NotFound || popup == Popup::Error) {
    if (millis() - popupTime >= POPUP_DURATION_MS) {
      ActivityResult res;
      res.isCancelled = true;
      setResult(std::move(res));
      finish();
    }
  }
}

void DictionaryWordSelectActivity::render(RenderLock&&) {
  if (popup == Popup::None) return;
  renderer.clearScreen();

  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  if (page) page->render(renderer, fontId, marginLeft, marginTop);
  scope.endScanAndPrewarm();
  if (page) page->render(renderer, fontId, marginLeft, marginTop);

  GUI.drawPopup(renderer, I18N.get(popupMsg));
}
