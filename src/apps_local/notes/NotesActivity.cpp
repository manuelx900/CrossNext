#include "NotesActivity.h"

#include <ESPmDNS.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>

#include <cstdio>

#include "../../CrossPointSettings.h"
#include "../../DevMode.h"
#include "../../activities/ActivityResult.h"
#include "../../activities/network/WifiSelectionActivity.h"
#include "../../activities/util/KeyboardEntryActivity.h"
#include "../../util/DeviceHostname.h"
#include "../../util/QrUtils.h"
#include "../Shelf.h"
#include "../ui/Dashboard.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ReadingToolsUi.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxIcons.h"
#include "../ui/ToyboxTheme.h"
#include "NotesCore.h"
#include "NotesSleep.h"
#include "NotesTimestamp.h"
#include "components/SideButtonNavigation.h"

namespace fui = freeink::ui;

namespace {

// A name is a filename, capped where the library caps one. KeyboardEntry's
// maxLength counts BYTES, which is what this is.
constexpr size_t kNameMax = 64;
constexpr size_t kLineMax = 200;

}  // namespace

std::unique_ptr<Activity> NotesActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<NotesActivity>(renderer, mappedInput);
}

std::unique_ptr<Activity> NotesActivity::createTodo(GfxRenderer& renderer, MappedInputManager& input) {
  auto app = makeUniqueNoThrow<NotesActivity>(renderer, input);
  if (app)
    app->todo = true;
  else
    LOG_ERR("NOTES", "OOM: To Do List");
  return app;
}

void NotesActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  // The Toybox cuts are registered by the app that wants them rather than by
  // main.cpp, so they cost no upstream surface. Without this every string on
  // every screen asks for a face the renderer does not have and draws nothing
  // at all -- not a box, not a fallback: nothing.
  toybox::ensureFonts(renderer);

  library_.filterTasks(todo);
  library_.orderRecent(!todo);
  if (!library_.begin()) {
    showNotice(tr(STR_UI_THE_CARD_WOULD_NOT_OPEN_SO_6F82D4));
    return;
  }
  if (todo)
    openFreeTasks();
  else
    openDeck();
}

// --- Rows ----------------------------------------------------------------

void NotesActivity::rebuildRows() {
  noteLayoutValid = false;
  const std::vector<notes::Entry>& entries = library_.entries();
  deckTallies_.clear();
  deckTallies_.reserve(entries.size());
  for (const notes::Entry& entry : entries) {
    char tally[28];
    std::snprintf(tally, sizeof(tally), "%d/%d", entry.done, entry.total);
    deckTallies_.emplace_back(entry.hasTasks ? tally : "");
  }
  notes::AsleepChoice sleepChoice;
  const bool hasSleep =
      SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::NOTE && notes::readAsleep(sleepChoice);
  // Pointers are taken in a SECOND pass. emplace_back can move every string it
  // has already stored, so a c_str() taken during the first loop points into a
  // buffer the vector has since freed.
  deckRows_.clear();
  deckRows_.reserve(entries.size());
  for (size_t i = 0; i < entries.size(); i++) {
    notesui::DeckItem row;
    row.title = entries[i].name.c_str();
    row.asleep = hasSleep && sleepChoice.name == entries[i].name;
    row.tally = deckTallies_[i].empty() ? nullptr : deckTallies_[i].c_str();
    row.preview = entries[i].preview.empty() ? nullptr : entries[i].preview.c_str();
    row.done = entries[i].done;
    row.total = entries[i].total;
    deckRows_.push_back(row);
  }

  taskTexts_.clear();
  rowLine_.clear();
  // NO BLANK ROWS, anywhere in the file, and no marker with nothing after it:
  // either would be an empty tick box, a hole in the list. The rule lives in
  // notes::drawnLines because the sleep screen draws these same rows.
  for (const size_t i : notes::drawnLines(doc_, lines_)) {
    rowLine_.push_back(i);
    taskTexts_.push_back(notes::textOf(doc_, lines_[i]));
  }
  taskRows_.clear();
  taskRows_.reserve(taskTexts_.size());
  for (size_t i = 0; i < taskTexts_.size(); i++) {
    notesui::Task row;
    row.text = taskTexts_[i].c_str();
    row.checked = lines_[rowLine_[i]].checked;
    taskRows_.push_back(row);
  }
}

bool NotesActivity::openIsPage() const { return notes::kindOf(lines_, newIsList_) == notes::Kind::Page; }

bool NotesActivity::anyDone() const {
  for (const notesui::Task& task : taskRows_) {
    if (task.checked) return true;
  }
  return false;
}

int NotesActivity::deckPageSize() {
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  target.setFont(4, toybox::kLibraryFontId);
  pixelui::bindFonts(target, renderer);
  static_assert(readingui::kReadingFont < fui::GfxRendererTarget::FONT_SLOTS);
  if (!todo) target.setFont(readingui::kReadingFont, toybox::kProseFontId);
  return notesui::deckCapacity(target.deviceContext(), todo);
}

notesui::NoteModel NotesActivity::noteModel() const {
  notesui::NoteModel model;
  model.todo = todo;
  model.freeTasks = openName_ == notes::kFreeTasksName;
  model.updated = updatedLabel;
  model.title = openName_ == notes::kFreeTasksName ? tr(STR_TODO_FREE_TASKS) : openName_.c_str();
  model.tasks = taskRows_.data();
  model.count = static_cast<int>(taskRows_.size());
  model.page = openIsPage();
  model.anyDone = anyDone();
  if (!model.page) {
    const notes::Counts c = notes::counts(lines_);
    model.done = c.done;
    model.total = c.marked;
  }
  return model;
}

const std::vector<int>& NotesActivity::notePageStarts() {
  if (noteLayoutValid && noteLayoutWidth == renderer.getScreenWidth() && noteLayoutHeight == renderer.getScreenHeight())
    return cachedNoteStarts;
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  target.setFont(4, toybox::kLibraryFontId);
  // The model as it will be DRAWN. A probe missing the kind measured a list's
  // tick boxes against a page's text width, and a probe missing the tally
  // measured against a band the progress strip was not standing in -- which
  // fits one row more than the screen draws, so the last item of a page went
  // missing while the page label counted it.
  pixelui::bindFonts(target, renderer);
  static_assert(readingui::kReadingFont < fui::GfxRendererTarget::FONT_SLOTS);
  if (!todo) target.setFont(readingui::kReadingFont, toybox::kProseFontId);
  // Move the existing page-start allocation into the activity; reuse until an edit/rotation.
  cachedNoteStarts = notesui::notePageStarts(target, target.deviceContext(), noteModel());
  noteLayoutWidth = renderer.getScreenWidth();
  noteLayoutHeight = renderer.getScreenHeight();
  noteLayoutValid = true;
  return cachedNoteStarts;
}

// Which page noteTop_ is on: the last page that starts at or before it. After
// an edit the pages move, and noteTop_ may point into the middle of one.
int NotesActivity::notePageOf(const std::vector<int>& starts) const {
  int page = 0;
  for (size_t p = 0; p < starts.size(); p++) {
    if (starts[p] <= noteTop_) page = static_cast<int>(p);
  }
  return page;
}

void NotesActivity::relabelDeck() {
  const int page = deckPageSize();
  const int count = static_cast<int>(deckRows_.size());
  deckPage_.clear();
  if (page <= 0 || count <= page) return;
  char label[32];
  std::snprintf(label, sizeof(label), "%d / %d", deckTop_ / page + 1, (count + page - 1) / page);
  deckPage_ = label;
}

void NotesActivity::relabelNote() {
  const std::vector<int>& starts = notePageStarts();
  const int page = notePageOf(starts);
  // Snapped to the start of its page, so the page drawn is a page the count
  // agrees exists rather than one that begins halfway down another.
  noteTop_ = starts[static_cast<size_t>(page)];
  notePage_.clear();
  if (starts.size() <= 1) return;
  char label[32];
  std::snprintf(label, sizeof(label), "%d / %d", page + 1, static_cast<int>(starts.size()));
  notePage_ = label;
}

// --- The sleep screen ----------------------------------------------------

namespace {
// What "stop showing a note while asleep" puts back: the mode the note
// replaced, unless nothing was recorded or what was recorded is Note itself,
// in which case the setting's own default.
uint8_t modeToRestore(const int previousMode) {
  if (previousMode >= 0 && previousMode < CrossPointSettings::SLEEP_SCREEN_MODE_COUNT &&
      previousMode != CrossPointSettings::SLEEP_SCREEN_MODE::NOTE) {
    return static_cast<uint8_t>(previousMode);
  }
  return static_cast<uint8_t>(CrossPointSettings::SLEEP_SCREEN_MODE::DARK);
}

// Put back both settings a note replaced. Quick Resume on Timeout is touched
// only when the choice recorded it.
void restoreSleepSettings(const notes::AsleepChoice& choice) {
  SETTINGS.sleepScreen = modeToRestore(choice.previousMode);
  if (choice.previousQuickResume == 1) {
    SETTINGS.quickResumeSleepScreen = CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_AFTER_TIMEOUT;
  } else if (choice.previousQuickResume == 0) {
    SETTINGS.quickResumeSleepScreen = CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_NEVER;
  }
  SETTINGS.saveToFile();
}
}  // namespace

bool NotesActivity::isShownAsleep() const {
  if (SETTINGS.sleepScreen != CrossPointSettings::SLEEP_SCREEN_MODE::NOTE) return false;
  notes::AsleepChoice choice;
  return notes::readAsleep(choice) && choice.name == openName_;
}

void NotesActivity::toggleAsleep(RenderLock&) {
  notes::AsleepChoice current;
  const bool hadChoice = notes::readAsleep(current);
  if (isShownAsleep()) {
    // Put back what was there. A choice with nothing recorded, or one that
    // recorded Note itself, falls back to the setting's own default.
    notes::clearAsleep();
    restoreSleepSettings(current);
    LOG_INF("NOTES", "'%s' no longer shown while asleep; sleep screen mode back to %d, quick resume on timeout %d",
            openName_.c_str(), SETTINGS.sleepScreen, SETTINGS.quickResumeSleepScreen);
  } else {
    notes::AsleepChoice choice;
    choice.name = openName_;
    // Switching from one note to another keeps the mode the FIRST note
    // replaced, so stopping later still puts back the person's own screen.
    const bool alreadyNote = SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::NOTE;
    choice.previousMode =
        alreadyNote ? (hadChoice ? current.previousMode : -1) : static_cast<int>(SETTINGS.sleepScreen);
    choice.previousQuickResume = alreadyNote ? (hadChoice ? current.previousQuickResume : -1)
                                             : static_cast<int>(SETTINGS.quickResumeSleepScreen);
    if (!notes::writeAsleep(choice)) {
      showNotice(tr(STR_UI_THE_CARD_WOULD_NOT_TAKE_THE_194E46));
      return;
    }
    SETTINGS.sleepScreen = CrossPointSettings::SLEEP_SCREEN_MODE::NOTE;
    // Quick resume on an idle sleep shows the last screen and skips the sleep
    // screen entirely, so a chosen note would never appear on the ordinary
    // sleep. The same trade the Wallpapers app makes, for the same reason.
    SETTINGS.quickResumeSleepScreen = CrossPointSettings::QUICK_RESUME_SLEEP_SCREEN::QUICK_RESUME_NEVER;
    SETTINGS.saveToFile();
    LOG_INF("NOTES", "'%s' shown while asleep (replaced sleep screen mode %d, quick resume on timeout %d)",
            openName_.c_str(), choice.previousMode, choice.previousQuickResume);
  }
  view_ = View::Note;
  interactionsReady_ = false;
  requestUpdate();
}

void NotesActivity::asleepRenamed(const std::string& from, const std::string& to) {
  notes::AsleepChoice choice;
  if (!notes::readAsleep(choice) || choice.name != from) return;
  choice.name = to;
  notes::writeAsleep(choice);
}

void NotesActivity::asleepDeleted(const std::string& name) {
  notes::AsleepChoice choice;
  if (!notes::readAsleep(choice) || choice.name != name) return;
  notes::clearAsleep();
  if (SETTINGS.sleepScreen == CrossPointSettings::SLEEP_SCREEN_MODE::NOTE) restoreSleepSettings(choice);
}

// --- Navigation ----------------------------------------------------------

void NotesActivity::openFreeTasks() {
  std::string text, message;
  if (!library_.load(notes::kFreeTasksName, text)) {
    if (Storage.exists("/notes/.crossplay-free-tasks.md") ||
        !library_.save(notes::kFreeTasksName, "- [ ] \n", message)) {
      showNotice(tr(STR_UI_THE_CARD_WOULD_NOT_TAKE_THE_194E46));
      return;
    }
    text = "- [ ] \n";
  }
  openName_ = notes::kFreeTasksName;
  doc_ = std::move(text);
  newIsList_ = true;
  noteTop_ = 0;
  view_ = View::Note;
  refreshFromDoc();
  interactionsReady_ = false;
  requestUpdate();
}

void NotesActivity::openDeck() {
  view_ = View::Deck;
  openName_.clear();
  doc_.clear();
  lines_.clear();
  deckTop_ = 0;
  // Re-read on every entry. The card can be edited from a computer between
  // sessions, and an app that trusts a cached list offers notes that are not
  // there any more.
  library_.scan();
  rebuildRows();
  relabelDeck();
  interactionsReady_ = false;
  requestUpdate();
}

void NotesActivity::reloadNote() {
  doc_.clear();
  library_.load(openName_, doc_);
  refreshFromDoc();
}

void NotesActivity::refreshFromDoc() {
  updatedLabel[0] = 0;
  notes::ModifiedDate modified;
  if (notes::decodeModified(library_.modified(openName_), modified)) {
    char date[28];
    dashboard::date(date, sizeof(date), modified.year, modified.month, modified.day);
    std::snprintf(updatedLabel, sizeof(updatedLabel), tr(STR_RT_UPDATED), date, modified.hour, modified.minute);
  }
  lines_ = notes::parse(doc_);
  rebuildRows();
  // relabelNote snaps noteTop_ onto a real page start. It is NOT compared with
  // the item count: on a note it counts LINES, and a note on its third page has
  // a noteTop_ past its number of paragraphs that is still perfectly valid.
  relabelNote();
}

void NotesActivity::openNote(const int index) {
  if (index < 0 || index >= library_.count()) return;
  openName_ = library_.entries()[index].name;
  noteTop_ = 0;
  reloadNote();
  view_ = View::Note;
  interactionsReady_ = false;
  requestUpdate();
}

void NotesActivity::showNotice(const std::string& text) {
  notice_ = text;
  view_ = View::Notice;
  interactionsReady_ = false;
  requestUpdate();
}

// --- Edits ---------------------------------------------------------------

void NotesActivity::toggleTask(const int index) {
  noteLayoutValid = false;
  if (index < 0 || index >= static_cast<int>(taskRows_.size())) return;
  // A ROW IS NOT A LINE any more: blank lines are skipped when the rows are
  // built, so row 4 can be line 6. Ticking by row index would tick a different
  // line than the one under the finger.
  const size_t line = rowLine_[static_cast<size_t>(index)];
  const std::string before = doc_;
  if (!notes::toggle(doc_, lines_[line])) {
    // A line written on a computer without the marker. Ticking it is how it
    // becomes one, rather than the app carrying a second kind of line forever.
    doc_.insert(lines_[line].begin, "- [x] ");
    lines_ = notes::parse(doc_);
  }

  // Written NOW, not on the way out. A tick a person saw and the card did not
  // is the failure mode of every app that saves on exit, and this one is used
  // one-handed in a shop with the power button under a thumb.
  std::string message;
  if (!library_.save(openName_, doc_, message)) {
    doc_ = before;  // the file is the truth; take back what RAM claimed
    lines_ = notes::parse(doc_);
    rebuildRows();
    showNotice(message);
    return;
  }
  // One row's mark changed and nothing else did. Rebuilding every row copied
  // every line of the note back out of the document on each tick.
  taskRows_[static_cast<size_t>(index)].checked = lines_[line].checked;
  requestUpdate();
}

void NotesActivity::switchKind() {
  noteLayoutValid = false;
  if (openName_ == notes::kFreeTasksName) return;
  const bool wasList = !openIsPage();
  const std::string before = doc_;
  if (wasList ? !notes::stripMarkers(doc_) : !notes::coerceToList(doc_)) {
    // Nothing to rewrite: an empty note has no line to carry a box. The kind it
    // will take is remembered instead, so the first line added gets it.
    newIsList_ = !wasList;
    view_ = View::Note;
    interactionsReady_ = false;
    requestUpdate();
    return;
  }
  std::string message;
  if (!library_.save(openName_, doc_, message)) {
    doc_ = before;  // the file is the truth; take back what RAM claimed
    lines_ = notes::parse(doc_);
    rebuildRows();
    showNotice(message);
    return;
  }
  newIsList_ = !wasList;
  noteTop_ = 0;
  refreshFromDoc();
  view_ = View::Note;
  interactionsReady_ = false;
  requestUpdate();
}

void NotesActivity::clearDone() {
  if (!anyDone()) return;
  const std::string before = doc_;
  notes::clearChecked(doc_);
  std::string message;
  if (!library_.save(openName_, doc_, message)) {
    doc_ = before;  // the file is the truth; take back what RAM claimed
    lines_ = notes::parse(doc_);
    rebuildRows();
    showNotice(message);
    return;
  }
  noteTop_ = 0;
  refreshFromDoc();
  view_ = View::Note;
  interactionsReady_ = false;
  requestUpdate();
}

bool NotesActivity::nameFitsBand(const std::string& name) {
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  target.setFont(4, toybox::kLibraryFontId);
  fui::TextStyle style;
  style.font = toybox::kBodyFont;
  const int16_t room = static_cast<int16_t>(target.deviceContext().width - 2 * toybox::kMargin - toybox::kHeaderHeight);
  return target.measureText(style.font, name.c_str(), style).width <= room;
}

void NotesActivity::askNewName() {
  // The prompt says which button was pressed. "NAME THIS NOTE" after tapping
  // + LIST is the app disagreeing with the control the finger just used.
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(
      renderer, mappedInput, newIsList_ ? tr(STR_UI_NAME_THIS_LIST_9E84DD) : tr(STR_UI_NAME_THIS_NOTE_5E2F02), "",
      kNameMax);
  if (!keyboard) {
    showNotice(tr(STR_UI_THERE_WAS_NOT_ENOUGH_MEMORY_TO_F5C32D));
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    interactionsReady_ = false;
    if (result.isCancelled) {
      requestUpdate();
      return;
    }
    const auto& entered = std::get<KeyboardResult>(result.data);
    // Refused HERE rather than silently shrinking the title bar later. The band
    // is chrome and carries one cut; a name that does not fit it is a name this
    // app will not make.
    if (notes::Library::sanitise(entered.text) == notes::kFreeTasksName ||
        !nameFitsBand(notes::Library::sanitise(entered.text))) {
      showNotice(tr(STR_UI_THAT_NAME_IS_TOO_LONG_TO_9B14CC));
      return;
    }
    std::string message;
    // Timed for the same reason the add is: "making a note took a while" is a
    // report nobody can act on without a number, and creating one is usually
    // the FIRST write of a session, which is where a per-write precondition
    // hides.
    const uint32_t startedAt = millis();
    if (!library_.create(entered.text, message)) {
      showNotice(message);
      return;
    }
    LOG_DBG("NOTES", "create: %ums", millis() - startedAt);
    // Straight into the note that was just made. Naming one and then having to
    // find it in the deck is a step nobody asked for.
    const std::string made = notes::Library::sanitise(entered.text);
    if (todo && !library_.save(made, "- [ ] \n", message)) {
      showNotice(message);
      return;
    }
    rebuildRows();
    for (int i = 0; i < library_.count(); i++) {
      if (library_.entries()[i].name == made) {
        openNote(i);
        return;
      }
    }
    openDeck();
  });
}

void NotesActivity::askRename() {
  if (openName_ == notes::kFreeTasksName) return;
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_UI_RENAME_475608), openName_, kNameMax);
  if (!keyboard) {
    showNotice(tr(STR_UI_THERE_WAS_NOT_ENOUGH_MEMORY_TO_F5C32D));
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    interactionsReady_ = false;
    if (result.isCancelled) {
      view_ = View::Note;
      requestUpdate();
      return;
    }
    const auto& entered = std::get<KeyboardResult>(result.data);
    if (notes::Library::sanitise(entered.text) == notes::kFreeTasksName ||
        !nameFitsBand(notes::Library::sanitise(entered.text))) {
      showNotice(tr(STR_UI_THAT_NAME_IS_TOO_LONG_TO_9B14CC));
      return;
    }
    std::string message;
    if (!library_.rename(openName_, entered.text, message)) {
      showNotice(message);
      return;
    }
    asleepRenamed(openName_, notes::Library::sanitise(entered.text));
    openName_ = notes::Library::sanitise(entered.text);
    reloadNote();
    view_ = View::Note;
    requestUpdate();
  });
}

void NotesActivity::askLine() {
  // THE COUNT IS THE RECEIPT. The keyboard reopens after each item so a list
  // can be written in one visit, and with a fixed title that read as OK doing
  // nothing at all: same screen, same words, empty field. The band now says how
  // many are on the list, so every OK visibly moves a number.
  char title[80];
  std::snprintf(title, sizeof(title), "%s  %d", openName_.c_str(), static_cast<int>(taskRows_.size()));
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, title, "", kLineMax);
  if (!keyboard) {
    showNotice(tr(STR_UI_THERE_WAS_NOT_ENOUGH_MEMORY_TO_F5C32D));
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    interactionsReady_ = false;
    if (result.isCancelled) {
      view_ = View::Note;
      requestUpdate();
      return;
    }
    const auto& entered = std::get<KeyboardResult>(result.data);
    if (entered.text.empty()) {
      view_ = View::Note;
      requestUpdate();
      return;
    }

    // A tick box on a list, a plain line on a note. The kind was decided when
    // the note was made and is written into the file by the first line, so this
    // never has to guess.
    const std::string before = doc_;
    if (!doc_.empty() && doc_.back() != '\n') doc_.push_back('\n');
    if (!openIsPage()) doc_ += "- [ ] ";
    doc_ += entered.text;
    doc_.push_back('\n');

    std::string message;
    // TIMED, permanently. "Adding an item is slow" was reported twice and both
    // times the answer had to be guessed from reading the code, because nothing
    // anywhere said how long the card took. These three numbers are what that
    // question needs: the write, the rebuild, and everything between OK and the
    // keyboard coming back.
    const uint32_t startedAt = millis();
    if (!library_.save(openName_, doc_, message)) {
      doc_ = before;
      lines_ = notes::parse(doc_);
      rebuildRows();
      showNotice(message);
      return;
    }
    const uint32_t savedAt = millis();
    // NOT reloadNote(): doc_ is what was just written, byte for byte. Reading
    // it back turned every OK into a second trip to the card.
    refreshFromDoc();
    // Onto the page the new line BEGINS on, so it is visibly there rather than
    // pages away. Not the last page: on a note a long paragraph flows, and one
    // added at the end can start on the page before the last.
    {
      fui::GfxRendererTarget target = toybox::makeTarget(renderer);
      target.setFont(4, toybox::kLibraryFontId);
      pixelui::bindFonts(target, renderer);
      static_assert(readingui::kReadingFont < fui::GfxRendererTarget::FONT_SLOTS);
      if (!todo) target.setFont(readingui::kReadingFont, toybox::kProseFontId);
      const int item = static_cast<int>(taskRows_.size()) - 1;
      const std::vector<int>& starts = notePageStarts();
      const int page = notesui::notePageOfItem(target, target.deviceContext(), noteModel(), item < 0 ? 0 : item);
      noteTop_ = starts[static_cast<size_t>(page)];
    }
    relabelNote();
    LOG_DBG("NOTES", "add: save %ums, rows %ums, %d items", savedAt - startedAt, millis() - savedAt,
            static_cast<int>(taskRows_.size()));
    view_ = View::Note;
    // STRAIGHT BACK TO THE KEYBOARD. A list is made of several things, and the
    // way out is Back or an empty line. One visit per item cost two activity
    // transitions and two full-screen repaints EACH -- six repaints to write a
    // three-line shopping list, with the keyboard torn down and rebuilt between
    // every word.
    askLine();
  });
}

// --- Typing from a phone -------------------------------------------------

void NotesActivity::startPhone() {
#ifndef SIMULATOR
  // NEVER launch the picker unconditionally: WifiSelectionActivity::startWifiScan
  // calls WiFi.disconnect() on every path, so an unguarded launch drops a
  // working association and shows a redundant chooser. Four other apps guard it
  // exactly this way.
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    startActivityForResult(makeUniqueNoThrow<WifiSelectionActivity>(renderer, mappedInput),
                           [this](const ActivityResult& result) {
                             if (result.isCancelled || WiFi.status() != WL_CONNECTED) {
                               showNotice(tr(STR_UI_TYPING_ON_YOUR_PHONE_NEEDS_WI_A2807C));
                               return;
                             }
                             startPhone();
                           });
    return;
  }
#endif

  // Developer Mode holds 80, 81 and UDP 8134 for as long as its toggle is on,
  // and Mario keeps a device on it. Two binds on one port fail in a way that
  // reads as "the screen is broken", so dev mode yields while this screen is up.
  // Every failure below leaves through stopPhone(), so the yield is released in
  // exactly ONE place no matter which way this goes wrong.
  devmode::pause();
  devPaused_ = true;

  server_ = makeUniqueNoThrow<CrossPointWebServer>(CrossPointWebServer::Surface::NotesOnly);
  if (!server_) {
    stopPhone();
    showNotice(tr(STR_UI_THERE_WAS_NOT_ENOUGH_MEMORY_TO_B6FD93));
    return;
  }
  server_->setNotesFile(std::string("/notes/") + openName_ + ".md", openName_, !openIsPage());
  server_->begin();
  // The simulator has no networking shim, so begin() never leaves the server
  // running there. The SCREEN is still drawn, because its layout is the half
  // that can be checked without hardware; what cannot be checked on a laptop is
  // said out loud in the commit rather than assumed.
#ifndef SIMULATOR
  if (!server_->isRunning()) {
    stopPhone();
    showNotice(tr(STR_UI_THE_READER_COULD_NOT_OPEN_ITS_AD19D9));
    return;
  }
#endif

#ifdef SIMULATOR
  // No radio here, so no name and no address to read off one. The screen is
  // still worth drawing: its layout is the half that can be checked without
  // hardware, and the server underneath it really does serve on the host.
  const bool mdnsUp = false;
  const std::string dotted = "127.0.0.1";
#else
  MDNS.end();
  const bool mdnsUp = MDNS.begin(devicehost::mdnsName());
  const std::string dotted = std::string(WiFi.localIP().toString().c_str());
#endif
  // THE CODE CARRIES THE ADDRESS, ALWAYS. It is generated from WiFi.localIP()
  // at the moment of drawing and depends on no service, so the only way it can
  // be wrong is DHCP moving this reader between the paint and the scan. The
  // NAME depends on a responder that can fail to start -- and this function
  // already knows when it has -- so encoding that would put a detected fault
  // into the one element a person cannot read.
  phoneUrl_ = "http://" + dotted + "/n";
#ifdef SIMULATOR
  phoneReadable_ = phoneUrl_;
  (void)mdnsUp;
#else
  phoneReadable_ = mdnsUp ? std::string("http://") + devicehost::mdnsName() + ".local/n" : phoneUrl_;
#endif
  phoneSaved_ = false;
  view_ = View::Phone;
  interactionsReady_ = false;
  requestUpdate();
}

void NotesActivity::stopPhone() {
  if (server_) {
    server_->stop();
    server_.reset();
#ifndef SIMULATOR
    MDNS.end();
#endif
  }
  // Guarded by the flag rather than by whether a server exists: the
  // out-of-memory path never got one, and resuming a yield this screen does not
  // hold drops the count out from under whoever does.
  if (devPaused_) {
    devPaused_ = false;
    devmode::resume();
  }
}

void NotesActivity::onExit() {
  stopPhone();
  Activity::onExit();
}

// --- Input ---------------------------------------------------------------

void NotesActivity::loop() {
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("APP-NAV", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;
  sectionnav::Input::Event navCommand{};
  const bool hasNavCommand = navigation.pop(navCommand);
  const bool navBack = hasNavCommand && navCommand.kind == sectionnav::Input::Kind::Back;
  const int navStep = hasNavCommand && (navCommand.kind == sectionnav::Input::Kind::Step ||
                                        navCommand.kind == sectionnav::Input::Kind::Section)
                          ? navCommand.amount
                          : 0;
  // Back is read on the per-frame path, above any "return unless a tap
  // arrived" guard, because the global back-swipe arrives as Button::Back and a
  // swipe is not a tap. host-tests/backgesture enforces this fork-wide.
  if (navBack) {
    switch (view_) {
      case View::Deck:
        // An app never names where Back goes; the shelf puts it back in
        // whichever folder opened it.
        shelf::leave(renderer, mappedInput);
        return;
      case View::Note:
        if (todo && openName_ == notes::kFreeTasksName)
          shelf::leave(renderer, mappedInput);
        else
          openDeck();
        return;
      case View::Phone:
        stopPhone();
        view_ = View::Note;
        interactionsReady_ = false;
        requestUpdate();
        return;
      case View::Menu:
      case View::Confirm:
      case View::Notice:
        view_ = openName_.empty() ? View::Deck : View::Note;
        interactionsReady_ = false;
        requestUpdate();
        return;
    }
  }

  const int swipe = interactionsReady_ && surfaceRevealed() ? sidebuttons::horizontalStep(mappedInput) : 0;
  const int heldPage = navStep;
  const bool down = swipe > 0 || heldPage > 0;
  const bool up = swipe < 0 || heldPage < 0;
  if (down || up) {
    if (view_ == View::Deck) {
      const int page = deckPageSize();
      const int count = static_cast<int>(deckRows_.size());
      const int direction = navStep ? navStep : down ? 1 : -1;
      const int next =
          page > 0 ? std::clamp(deckTop_ + direction * page, 0, std::max(0, (count - 1) / page * page)) : 0;
      if (page > 0 && count > page && next != deckTop_) {
        deckTop_ = next;
        relabelDeck();
        interactionsReady_ = false;
        requestUpdate();
      }
      return;
    }
    if (view_ == View::Note) {
      const std::vector<int>& starts = notePageStarts();
      const int page = notePageOf(starts);
      const int direction = navStep ? navStep : down ? 1 : -1;
      const int next = std::clamp(page + direction, 0, std::max(0, int(starts.size()) - 1));
      if (!starts.empty() && next != page) {
        noteTop_ = starts[static_cast<size_t>(next)];
        relabelNote();
        interactionsReady_ = false;
        requestUpdate();
      }
      return;
    }
  }

  if (server_ && server_->isRunning()) {
    // Pumped from loop() rather than a task: there are no background threads in
    // this firmware, and a blocking handler on the render path is what makes a
    // screen look frozen.
    for (int i = 0; i < 8 && server_->isRunning(); ++i) server_->handleClient();
    if (server_->takeNotesChanged()) {
      reloadNote();
      phoneSaved_ = true;
      interactionsReady_ = false;
      requestUpdate();
    }
  }

  int x = 0;
  int y = 0;
  // Interactions::route() refuses a tap routed against a table the panel has
  // not shown yet, which is what stops a tap aimed at the screen underneath
  // from landing on the one that replaced it during a 0.3-2s repaint.
  if (!sidebuttons::poll(interactions_, mappedInput, x, y, !todo) || !interactionsReady_ || !surfaceRevealed()) return;
  fui::InputSnapshot input{};
  input.touchReleased = true;
  input.touchX = static_cast<int16_t>(x);
  input.touchY = static_cast<int16_t>(y);
  const fui::ActionEvent action = sidebuttons::route(interactions_, input, mappedInput, *this, !todo);

  switch (action.action) {
    case notesui::ActionSort:
      if (view_ != View::Deck || todo) return;
      sortRecent = !sortRecent;
      library_.orderRecent(sortRecent);
      deckTop_ = 0;
      rebuildRows();
      relabelDeck();
      interactionsReady_ = false;
      requestUpdate();
      return;
    case notesui::ActionFreeTasks:
      if (todo) openFreeTasks();
      return;
    case notesui::ActionTodoLists:
      if (todo) openDeck();
      return;
    case notesui::ActionPage: {
      if (view_ == View::Deck) {
        const int next = deckTop_ + action.value * deckPageSize();
        if (next < 0 || next >= int(deckRows_.size())) return;
        deckTop_ = next;
        relabelDeck();
      } else if (view_ == View::Note) {
        const auto& starts = notePageStarts();
        const int next = notePageOf(starts) + action.value;
        if (next < 0 || next >= int(starts.size())) return;
        noteTop_ = starts[next];
        relabelNote();
      } else
        return;
      interactionsReady_ = false;
      requestUpdate();
      return;
    }
    case notesui::ActionOpenNote:
      openNote(action.value);
      return;
    case notesui::ActionNewList:
      if (!todo) return;
      newIsList_ = true;
      askNewName();
      return;
    case notesui::ActionNewPage:
      if (todo) return;
      newIsList_ = false;
      askNewName();
      return;
    case notesui::ActionToggleTask:
      toggleTask(action.value);
      return;
    case notesui::ActionAddLine:
      askLine();
      return;
    case notesui::ActionMenu:
      if (openName_.empty() || (todo && openName_ == notes::kFreeTasksName)) return;
      view_ = View::Menu;
      interactionsReady_ = false;
      requestUpdate();
      return;
    case notesui::ActionClearDone:
      clearDone();
      return;
    case notesui::ActionSwitchKind:
      switchKind();
      return;
    case notesui::ActionShowAsleep:
      toggleAsleep(lock);
      return;
    case notesui::ActionRename:
      askRename();
      return;
    case notesui::ActionUsePhone:
      startPhone();
      return;
    case notesui::ActionDelete:
      // On the menu this OPENS the confirm; on the confirm it does the thing.
      // One action id for both, because the confirm's own DELETE IT is the
      // only control that may destroy a note and it lives on a screen the
      // person had to arrive at deliberately.
      if (view_ == View::Menu) {
        view_ = View::Confirm;
        interactionsReady_ = false;
        requestUpdate();
        return;
      }
      if (!library_.remove(openName_)) {
        showNotice(tr(STR_UI_THE_CARD_WOULD_NOT_OPEN_SO_6F82D4));
        return;
      }
      asleepDeleted(openName_);
      openDeck();
      return;
    case notesui::ActionDismiss:
      stopPhone();
      view_ = openName_.empty() ? View::Deck : View::Note;
      interactionsReady_ = false;
      requestUpdate();
      return;
    default:
      return;
  }
}

// --- Render --------------------------------------------------------------

void NotesActivity::render(RenderLock&&) {
  if (todo) interactions_.setFocusedIndex(-1);
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  target.setFont(4, toybox::kLibraryFontId);
  pixelui::bindFonts(target, renderer);
  static_assert(readingui::kReadingFont < fui::GfxRendererTarget::FONT_SLOTS);
  if (!todo) target.setFont(readingui::kReadingFont, toybox::kProseFontId);
  const fui::InputSnapshot noInput{};
  interactionsReady_ = false;
  const auto device = target.deviceContext();
  toybox::Frame frame(target, device, noInput, interactions_, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);

  switch (view_) {
    case View::Deck: {
      notesui::DeckModel model;
      model.todo = todo;
      model.recent = sortRecent;
      model.items = deckRows_.data();
      model.count = static_cast<int>(deckRows_.size());
      model.firstVisible = deckTop_;
      model.pageLabel = deckPage_.empty() ? nullptr : deckPage_.c_str();
      notesui::buildDeck(screen, model);
      break;
    }
    case View::Note: {
      notesui::NoteModel model = noteModel();
      model.firstVisible = noteTop_;
      model.pageLabel = notePage_.empty() ? nullptr : notePage_.c_str();
      model.menuIcon = &icon_go_settings_32;
      notesui::buildNote(screen, model);
      break;
    }
    case View::Menu: {
      notesui::MenuModel model;
      model.title = openName_ == notes::kFreeTasksName ? tr(STR_TODO_FREE_TASKS) : openName_.c_str();
      model.menuIcon = &icon_go_settings_32;
      model.anyDone = anyDone();
      model.isList = !openIsPage();
      model.shownAsleep = isShownAsleep();
      model.todo = todo;
      notesui::buildMenu(screen, model);
      break;
    }
    case View::Confirm: {
      notesui::ConfirmModel model;
      model.title = openName_ == notes::kFreeTasksName ? tr(STR_TODO_FREE_TASKS) : openName_.c_str();
      model.menuIcon = &icon_go_settings_32;
      model.prose = tr(STR_UI_DELETE_THIS_NOTE_AND_EVERYTHING_WRITTEN_D1E983);
      model.reference = !todo;
      notesui::buildConfirm(screen, model);
      break;
    }
    case View::Phone: {
      notesui::PhoneModel model;
      model.title = openName_ == notes::kFreeTasksName ? tr(STR_TODO_FREE_TASKS) : openName_.c_str();
      model.menuIcon = &icon_go_settings_32;
      model.url = phoneUrl_.c_str();
      model.readable = phoneReadable_.c_str();
      model.saved = phoneSaved_;
      const fui::Rect qr = notesui::buildPhone(screen, model);
      QrUtils::drawQrCode(renderer, Rect{qr.x, qr.y, qr.width, qr.height}, phoneUrl_);
      break;
    }
    case View::Notice: {
      notesui::ConfirmModel model;
      model.title = openName_.empty() ? tr(STR_UI_NOTES_E69FC1) : openName_.c_str();
      model.prose = notice_.c_str();
      notesui::buildNotice(screen, model);
      break;
    }
  }

  interactionsReady_ = true;
  // The buffer records overflow and this reports it; a screen that silently
  // drops its last three controls is how Connections lost all its buttons.
  toybox::reportOverflow(interactions_, tr(STR_NOTES_APP), &renderer);
  renderer.displayBuffer();
}

uint32_t NotesActivity::surfaceMeaning() const {
  uint32_t h = paintclock::kMeaningSeed;
  h = paintclock::mixMeaning(h, uint32_t(view_));
  h = paintclock::mixMeaning(h, sortRecent);
  h = paintclock::mixMeaning(h, uint32_t(deckTop_));
  h = paintclock::mixMeaning(h, uint32_t(noteTop_));
  for (const unsigned char c : openName_) h = paintclock::mixMeaning(h, c);
  return h;
}
