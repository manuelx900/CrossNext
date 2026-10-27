#include "FlashcardsActivity.h"

#include <FsHelpers.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../Shelf.h"
#include "../study/StudyActivity.h"
#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/PixelMenuTheme.h"
#include "FlashHistoryActivity.h"
#include "FlashSettingsActivity.h"
#include "components/SideButtonNavigation.h"
#include "components/icons/listIcons.h"
namespace fui = freeink::ui;
namespace {
bool supported(std::string_view name) {
  for (const char* ext : {".apkg", ".colpkg", ".anki2", ".anki21", ".csv", ".tsv", ".txt"})
    if (FsHelpers::checkFileExtension(name, ext)) return true;
  return false;
}
bool inside(fui::Rect r, int x, int y) { return x >= r.x && y >= r.y && x < r.right() && y < r.bottom(); }
}  // namespace
std::unique_ptr<Activity> FlashcardsActivity::create(GfxRenderer& r, MappedInputManager& i) {
  auto app = makeUniqueNoThrow<FlashcardsActivity>(r, i);
  if (!app) LOG_ERR("FLASH", "OOM: %u bytes", unsigned(sizeof(FlashcardsActivity)));
  return app;
}
void FlashcardsActivity::onEnter() {
  Activity::onEnter();
  if (!Storage.exists("/FLASHCARDS") && Storage.exists("/flashcards")) path = "/flashcards";
  path.reserve(512);
  selectedPath.reserve(512);
  entries.reserve(128);
  error = !Storage.ensureDirectoryExists(path.c_str());
  scan();
}
void FlashcardsActivity::onExit() {
  importer.reset();
  Activity::onExit();
}
void FlashcardsActivity::scan() {
  entries.clear();
  page = 0;
  error = false;
  HalFile directory = Storage.open(path.c_str());
  if (!directory) {
    error = true;
    requestUpdate();
    return;
  }
  for (auto file = directory.openNextFile(); file; file = directory.openNextFile()) {
    file.getName(buffer, sizeof(buffer));
    if (!buffer[0] || buffer[0] == '.') continue;
    if (!file.isDirectory() && !supported(buffer)) continue;
    if (entries.size() == 128) break;
    flashcards::DeckInfo item;
    item.name = buffer;
    item.source = path + "/" + buffer;
    item.folder = file.isDirectory();
    if (item.folder) {
      if (catalog.describe(item.source.c_str(), item)) item.folder = false;
    } else if (catalog.findImported(item.source.c_str(), cache, sizeof(cache)))
      catalog.describe(cache, item);
    if (!item.ready && !item.folder) {
      const auto dot = item.name.rfind('.');
      if (dot != std::string::npos) item.name.resize(dot);
    }
    entries.push_back(std::move(item));
  }
  std::sort(entries.begin(), entries.end(), [](const flashcards::DeckInfo& a, const flashcards::DeckInfo& b) {
    if (a.folder != b.folder) return a.folder > b.folder;
    return FsHelpers::naturalLess(a.name, b.name);
  });
  requestUpdate();
}
void FlashcardsActivity::onBackButton() {
  switch (view) {
    case View::Import:
      importer.reset();
      view = View::Browse;
      requestUpdate();
      return;
    case View::Help:
    case View::Prepare:
    case View::Result:
      view = View::Browse;
      scan();
      return;
    case View::Browse:
      if (path != "/FLASHCARDS" && path != "/flashcards") {
        path.resize(path.rfind('/'));
        scan();
      } else
        shelf::leave(renderer, mappedInput);
      return;
  }
}
void FlashcardsActivity::activateIndex(int n) {
  if (view != View::Browse || n < 0 || size_t(n) >= entries.size()) return;
  const auto& entry = entries[n];
  selectedPath = entry.source;
  if (entry.ready && entry.source == entry.directory) {
    openDeck(entry.directory.c_str());
    return;
  }
  if (selectedPath.size() > 511) {
    error = true;
    requestUpdate();
    return;
  }
  if (entry.folder) {
    std::snprintf(buffer, sizeof(buffer), "%s/meta.dat", selectedPath.c_str());
    if (Storage.exists(buffer)) {
      openDeck(selectedPath.c_str());
      return;
    }
    path = selectedPath;
    scan();
    return;
  }
  importStartedMs = millis();
  lastProgressMs = 0;
  progressPublished = false;
  imported = skipped = 0;
  importUi = {};
  view = View::Prepare;
  interactionsReady = false;
  error = false;
  requestUpdate();
}
void FlashcardsActivity::openDeck(const char* directory) {
  auto next = StudyActivity::openInstalled(renderer, mappedInput, directory);
  if (!next) {
    error = true;
    view = View::Result;
    requestUpdate();
    return;
  }
  startActivityForResult(std::move(next), [this](const ActivityResult&) {
    view = View::Browse;
    scan();
  });
}
bool FlashcardsActivity::prepare() {
  if (!catalog.resolve(selectedPath.c_str(), cache, sizeof(cache), {this, progressTrampoline})) return false;
  std::snprintf(message, sizeof(message), "%s/meta.dat", cache);
  if (Storage.exists(message)) {
    RenderLock lock(*this);
    openDeck(cache);
    return true;
  }
  if (!Storage.ensureDirectoryExists("/.crosspoint/flashcards")) return false;
  // Bounded text workspace belongs to import in PSRAM and is released before
  // the study activity allocates its reusable long-note buffer.
  importer = makeUniqueNoThrow<flashcards::Importer>();
  if (!importer) {
    LOG_ERR("FLASH", "OOM: importer %u bytes", unsigned(sizeof(flashcards::Importer)));
    return false;
  }
  importer->setProgressReporter({this, progressTrampoline});
  const auto slash = selectedPath.rfind('/');
  if (!importer->begin(selectedPath.c_str(), cache, selectedPath.c_str() + slash + 1)) {
    importer.reset();
    return false;
  }
  {
    RenderLock lock(*this);
    view = View::Import;
  }
  return true;
}
bool FlashcardsActivity::handleCustomInput() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onBackButton();
    return true;
  }
  if (interactionsReady && (view == View::Prepare || view == View::Import)) {
    int x, y;
    if (sidebuttons::poll(interactions, mappedInput, x, y)) {
      fui::InputSnapshot tap;
      tap.touchReleased = true;
      tap.touchX = x;
      tap.touchY = y;
      if (sidebuttons::route(interactions, tap, mappedInput, *this).action == flashui::Back) {
        onBackButton();
        return true;
      }
    }
  }
  if (view == View::Prepare || view == View::Import) return true;
  if (!interactionsReady) return true;
  if (view == View::Browse || view == View::Help) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      if (view == View::Help)
        helpPage = std::clamp(helpPage + direction, 0, 1);
      else
        page = std::clamp(page + direction, 0,
                          std::max(0, (int(entries.size()) - 1) /
                                          flashui::libraryCapacity(toybox::makeTarget(renderer).deviceContext())));
      requestUpdate();
      return true;
    }
  }
  int x, y;
  if (sidebuttons::poll(interactions, mappedInput, x, y)) {
    fui::InputSnapshot input;
    input.touchReleased = true;
    input.touchX = x;
    input.touchY = y;
    const auto event = sidebuttons::route(interactions, input, mappedInput, *this);
    if (event.action == flashui::Settings) {
      auto child = FlashSettingsActivity::create(renderer, mappedInput);
      if (child) startActivityForResult(std::move(child), [this](const ActivityResult&) { requestUpdate(); });
    } else if (event.action == flashui::OpenDeck)
      activateIndex(page * flashui::libraryCapacity(toybox::makeTarget(renderer).deviceContext()) + event.value);
    else if (event.action == flashui::History) {
      auto child = FlashHistoryActivity::create(renderer, mappedInput);
      if (child) startActivityForResult(std::move(child), [this](const ActivityResult&) { scan(); });
    } else if (event.action == flashui::Help) {
      view = View::Help;
      helpPage = 0;
      requestUpdate();
    } else if (event.action == flashui::Page) {
      if (view == View::Help)
        helpPage = std::clamp(helpPage + event.value, 0, 1);
      else
        page = std::clamp(page + event.value, 0,
                          std::max(0, (int(entries.size()) - 1) /
                                          flashui::libraryCapacity(toybox::makeTarget(renderer).deviceContext())));
      requestUpdate();
    } else if (event.action == flashui::Back)
      onBackButton();
    else if (event.action == flashui::ViewSession)
      openDeck(cache);
  }
  const int swipe = interactions.routable() ? sidebuttons::swipeStep(mappedInput) : 0;
  if ((view == View::Browse || view == View::Help) &&
      (mappedInput.wasReleased(MappedInputManager::Button::Down) ||
       mappedInput.wasReleased(MappedInputManager::Button::Up) || swipe)) {
    const int delta = (mappedInput.wasReleased(MappedInputManager::Button::Down) || swipe > 0) ? 1 : -1;
    if (view == View::Help)
      helpPage = std::clamp(helpPage + delta, 0, 1);
    else
      page = std::clamp(page + delta, 0,
                        std::max(0, (int(entries.size()) - 1) /
                                        flashui::libraryCapacity(toybox::makeTarget(renderer).deviceContext())));
    requestUpdate();
  }
  return true;
}
void FlashcardsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer, true);
  target.setFont(4, UI_10_FONT_ID);
  fui::InputSnapshot none;
  toybox::Frame frame(target, target.deviceContext(), none, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  interactionsReady = false;
  if (view == View::Browse) {
    target.setFont(4, SMALL_FONT_ID);
    flashui::LibraryModel model;
    model.error = error;
    model.totalDecks = entries.size();
    const int capacity = flashui::libraryCapacity(target.deviceContext());
    model.pages = std::max(1, (int(entries.size()) + capacity - 1) / capacity);
    page = std::clamp(page, 0, model.pages - 1);
    model.page = page;
    for (const auto& d : entries) model.today += d.due + d.fresh;
    for (int n = page * capacity; n < int(entries.size()) && n < page * capacity + capacity; ++n) {
      const auto& d = entries[n];
      deckRows[model.count++] = {d.name.c_str(), d.total, d.due, d.fresh, d.ready, d.folder};
    }
    model.rows = deckRows;
    flashui::library(screen, model);
  } else if (view == View::Help)
    flashui::help(screen, helpPage);
  else {
    pixelui::begin(screen);
    pixelui::Header h;
    h.title = tr(STR_FLASHCARDS_APP);
    h.back = true;
    pixelui::header(screen, h);
    backui::publish(true, pixelui::geometry(screen.device()).back);
    const auto body = screen.body();
    if (view == View::Prepare || view == View::Import) {
      flashui::importing(screen, importUi);
    } else {
      const char* text = tr(STR_FLASHCARDS_IMPORT_ERROR);
      if (!error) {
        std::snprintf(message, sizeof(message), tr(STR_FLASHCARDS_IMPORT_DONE), unsigned(imported), unsigned(skipped));
        text = message;
      }
      fittext::draw(screen.target(), fui::makeRect(body.x, body.y, body.width, body.height - 144), text, 4, 10);
    }
    if (view == View::Result && !error)
      dashboard::action(screen, fui::makeRect(body.x, body.bottom() - 124, body.width, 56), tr(STR_FLASH_START_REVIEW),
                        flashui::ViewSession, &extra_dashboard_play_32, true);
    dashboard::action(screen, fui::makeRect(body.x, body.bottom() - 56, body.width, 56), tr(STR_FLASH_BACK_DECKS),
                      flashui::Back, &extra_dashboard_cards_32);
  }
  interactionsReady = true;
  toybox::reportOverflow(interactions, "Flashcards", &renderer);
  renderer.displayBuffer();
}
void FlashcardsActivity::progressTrampoline(void* context, const flashcards::ImportProgress& progress) {
  static_cast<FlashcardsActivity*>(context)->updateProgress(progress);
}
void FlashcardsActivity::updateProgress(const flashcards::ImportProgress& progress) {
  const uint32_t now = millis();
  if (progressPublished && now - lastProgressMs < 1000) return;
  RenderLock lock(*this);
  lastProgressMs = millis();
  progressPublished = true;
  importUi.progress = progress;
  importUi.elapsedMs = lastProgressMs - importStartedMs;
  importUi.cards = importer ? importer->count() : 0;
  importUi.skipped = importer ? importer->skippedCount() : 0;
  ++importUi.tick;
  requestUpdate(true);
}
void FlashcardsActivity::loop() {
  View work;
  bool ready;
  {
    RenderLock lock(*this);
    handleCustomInput();
    work = view;
    ready = interactionsReady;
  }
  // Storage/conversion runs on this loop task, without holding the render
  // mutex. Synchronous progress callbacks publish small locked UI snapshots.
  switch (work) {
    case View::Prepare: {
      if (!ready) return;
      const bool ok = prepare();
      RenderLock lock(*this);
      if (!ok) {
        error = true;
        view = View::Result;
      }
      requestUpdate();
      return;
    }
    case View::Import: {
      const auto result = importer->step();
      if (result == flashcards::ImportResult::Busy) return;
      RenderLock lock(*this);
      imported = importer->count();
      skipped = importer->skippedCount();
      error = result == flashcards::ImportResult::Failed;
      importer.reset();
      view = View::Result;
      if (!error) {
        std::snprintf(message, sizeof(message), "%s/source.txt", cache);
        HalFile file;
        if (!Storage.openFileForWrite("FLASH", message, file) ||
            file.write(selectedPath.c_str(), selectedPath.size()) != selectedPath.size())
          error = true;
      }
      requestUpdate();
      return;
    }
    case View::Browse:
    case View::Help:
    case View::Result:
      return;
  }
}
