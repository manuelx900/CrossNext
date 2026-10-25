#include "MurdleActivity.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstddef>
#include <cstring>

#include "components/SideButtonNavigation.h"

// GUI, for drawButtonHints. Included directly rather than inherited from
// Toybox.h: a header pulled in only for a macro reads as unused to clangd.
#include "../../components/UITheme.h"
#include "../../fontIds.h"
#include "../Shelf.h"
#include "../ui/PixelMenu.h"
#include "../ui/PixelMenuTheme.h"
#include "../ui/ToyboxFonts.h"
#include "../ui/ToyboxSeed.h"
#include "../ui/ToyboxTheme.h"
#include "MurdleCast.h"
#include "MurdlePeople.h"
#include "MurdleSave.h"
#include "MurdleSession.h"
#include "MurdleText.h"

namespace {

namespace fui = freeink::ui;
namespace ui = murdleui;

constexpr char kSavePath[] = "/.crosspoint/murdle.sav";

// How many different seeds to ask for a case before admitting defeat. Each one
// is already 64 attempts inside the generator, so reaching the end of this means
// something is wrong with the tier rather than with the luck of the draw -- and
// that is worth a log line and a trip to the menu. See generateCase().
constexpr int kSeedAttempts = 8;

using SaveState = murdlesave::State;
constexpr char kSavePending[] = "/.crosspoint/murdle.tmp", kSaveBackup[] = "/.crosspoint/murdle.bak";
bool readSaveRecord(const char* path, SaveState& state, bool* future = nullptr) {
  HalFile file;
  if (!Storage.openFileForRead("MRDL", path, file)) return false;
  uint8_t bytes[murdlesave::kBytes];
  const auto size = file.size();
  if (!size || size > sizeof(bytes) || file.read(bytes, size) != static_cast<int>(size)) {
    // A future record may be larger than this version's workspace.
    if (future && file.seekSet(0) && file.read(bytes, 1) == 1 && bytes[0] > murdlesave::kVersion) *future = true;
    return false;
  }
  if (future && bytes[0] > murdlesave::kVersion) *future = true;
  const bool valid = murdlesave::decode(bytes, size, state);
  if (!valid) LOG_ERR("MRDL", "Unreadable save %s: version %u, %u bytes", path, unsigned(bytes[0]), unsigned(size));
  return valid;
}

// Only what the player entered is stored. The crosses that follow from it are
// derived on the way to the screen and were never in a cell to be saved, so a
// restored board rebuilds them from the same assertions -- there is no second
// copy that could come back stale.
void packMarks(const murdle::Marks& grid, const murdle::Shape shape, uint8_t* out, const size_t cap) {
  std::memset(out, 0, cap);
  int bit = 0;
  for (int a = 0; a < shape.cats; ++a) {
    for (int b = a + 1; b < shape.cats; ++b) {
      for (int ia = 0; ia < shape.items; ++ia) {
        for (int ib = 0; ib < shape.items; ++ib, ++bit) {
          const size_t byte = static_cast<size_t>(bit) / 4;
          if (byte >= cap) return;
          const uint8_t value = static_cast<uint8_t>(grid.entered(a, ia, b, ib));
          out[byte] = static_cast<uint8_t>(out[byte] | (value << ((bit % 4) * 2)));
        }
      }
    }
  }
}

void unpackMarks(const uint8_t* in, const size_t cap, const murdle::Shape shape, murdle::Marks& grid) {
  grid.reset(shape);
  int bit = 0;
  for (int a = 0; a < shape.cats; ++a) {
    for (int b = a + 1; b < shape.cats; ++b) {
      for (int ia = 0; ia < shape.items; ++ia) {
        for (int ib = 0; ib < shape.items; ++ib, ++bit) {
          const size_t byte = static_cast<size_t>(bit) / 4;
          if (byte >= cap) return;
          const uint8_t value = static_cast<uint8_t>((in[byte] >> ((bit % 4) * 2)) & 3u);
          if (value != 0) grid.enter(a, ia, b, ib, static_cast<murdle::Mark>(value));
        }
      }
    }
  }
}

}  // namespace

std::unique_ptr<Activity> MurdleActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput) {
  return makeUniqueNoThrow<MurdleActivity>(renderer, mappedInput);
}

void MurdleActivity::onEnter() {
  toybox::ensureFonts(renderer);
  saveWritable = Storage.ready() && Storage.ensureDirectoryExists("/.crosspoint");
  const bool existing = Storage.exists(kSavePath) || Storage.exists(kSaveBackup);
  if (!loadState()) {
    hasCase = false;
    tier = murdle::Tier::Elementary;
    if (!saveWritable || existing) {
      saveWritable = false;
      saveFailed = saveNotice = true;
    }
  }
  view = View::Menu;
  dirty = false;
  lastSaveAttempt = millis();
  requestUpdate();
}

void MurdleActivity::onExit() {
  flushSave();
  scratch.reset();
}

void MurdleActivity::goToMenu() {
  view = View::Menu;
  notice[0] = '\0';
  flushSave();
  requestUpdate();
}

// ---------------------------------------------------------------------------
// The new-case funnel

void MurdleActivity::requestNewCase() {
  if (!saveWritable) {
    saveNotice = true;
    interactionsReady = false;
    requestUpdate();
    return;
  }
  if (hasCase && !solved && murdlesession::lives(wrongAccusations) > 0) {
    view = View::ConfirmNew;
    requestUpdate();
    return;
  }
  generatePending = true;
  view = View::Case;
  face = ui::Face::Clues;
  page = 0;
  requestUpdate();
}

void MurdleActivity::generateCase() {
  if (!scratch) {
    scratch = makeUniqueNoThrow<murdle::Scratch>();
    if (!scratch) {
      LOG_ERR("MRDL", "OOM: generator scratch (%u bytes)", static_cast<unsigned>(sizeof(murdle::Scratch)));
      goToMenu();
      return;
    }
  }

  // millis() is the only entropy on this device that differs between two boots.
  // Mixed rather than used raw: the timer advances in even steps and the low
  // bit of a raw reading is not random.
  seed = pendingCampaign >= 0 ? murdlecampaign::kSeeds[pendingCampaign] : toybox::seed();
  if (pendingCampaign >= 0) tier = murdlecampaign::tier(pendingCampaign);
  const murdle::Shape shape = murdle::shapeOf(tier);

  const uint32_t startedAt = millis();

  // A refusal is a refusal of THIS SEED, not of the tier, so try another one.
  //
  // generate() will not return a case unless it has exactly one solution, needs
  // every clue it carries, and can be finished with pencil rules alone. That
  // guarantee is absolute and is the point of the whole generator, so it never
  // bends -- but it does mean a seed whose cast or solution cannot support such
  // a case gets nothing back. One seed in three hundred did exactly that during
  // development. The old code took that as fatal and bounced the player to the
  // menu with no explanation and only a log line to show for it, which is a
  // rare bug that would have been extremely hard to hear about second hand.
  //
  // Seeds are arbitrary. Drawing a different one costs nothing a player can
  // perceive and keeps the guarantee where it belongs, in the generator.
  uint8_t cast[murdle::kMaxCats][murdle::kMaxItems];
  bool built = false;
  for (int attempt = 0; attempt < (pendingCampaign >= 0 ? 1 : kSeedAttempts) && !built; ++attempt) {
    if (attempt > 0) seed = seed * 1664525u + 1013904223u;
    if (!murdle::drawCast(seed, shape, cast)) continue;
    built = murdle::generate(tier, seed, cast, murdle::attrMasksFor(cast, shape), *scratch, puzzle);
  }
  if (!built) {
    LOG_ERR("MRDL", "No fair case for tier %d after %d seeds", static_cast<int>(tier), kSeedAttempts);
    goToMenu();
    return;
  }
  scratch.reset();

  puzzle.peopleSeed = pendingCampaign >= 0 ? (seed ^ 0x9E3779B9u) | 1u : toybox::seed() | 1u;
  resetCaseProgress();
  campaignCase = pendingCampaign;
  pendingCampaign = -1;
  if (campaignCase >= 0)
    caseNumber = campaignCase + 1;
  else
    ++caseNumber;
  // The milliseconds are the point of this line, not decoration.
  //
  // Generation is a single blocking call in loop(), so its cost is the cost of
  // the whole activity going unresponsive, and the task watchdog fires at five
  // seconds. The generator this replaced took an estimated 12 to 23 seconds a
  // case on Hard Boiled -- not slow, a reboot -- and nothing on the device
  // would have said so; it was found by timing the host build and scaling.
  // That is a logging bug as much as a performance one. This number turns the
  // estimate into a measurement the first time anybody plays, and turns a
  // future regression into something visible rather than something Mario has
  // to describe over the phone.
  LOG_INF("MRDL", "Case %d: tier %d, %d clues, %d rounds, %u ms", caseNumber, static_cast<int>(tier), puzzle.clueCount,
          puzzle.rounds, static_cast<unsigned>(millis() - startedAt));
}

void MurdleActivity::resetCaseProgress() {
  gridPage = infoCategory = 0;
  showSolution = undoable = false;
  lastCell.catA = -1;
  marks.reset(puzzle.shape);
  hasCase = true;
  solved = false;
  struck = 0;
  wrongAccusations = 0;
  page = 0;
  face = ui::Face::Clues;
  for (int c = 0; c < murdle::kMaxCats; ++c) picks[c] = ui::AccuseModel::kNothingPicked;
  dirty = true;
  flashOnNextPaint = true;
  notice[0] = '\0';
}

void MurdleActivity::openCase() {
  if (!hasCase) {
    pendingCampaign = std::min(murdlecampaign::kCaseCount - 1, campaign.next());
    requestNewCase();
    return;
  }
  showSolution = false;
  view = solved || murdlesession::lives(wrongAccusations) == 0 ? View::Verdict : View::Case;
  requestUpdate();
}

void MurdleActivity::resetAllProgress() {
  campaign = {};
  campaignCase = pendingCampaign = -1;
  campaignPage = campaignSelected = caseNumber = 0;
  campaignTier = tier = murdle::Tier::Elementary;
  hasCase = solved = generatePending = showSolution = undoable = false;
  solvedCount = wrongCount = wrongAccusations = 0;
  record = struck = seed = 0;
  page = gridPage = infoCategory = 0;
  dirty = true;
  flushSave();
  view = View::Cases;
  flashOnNextPaint = true;
  requestUpdate();
}

// ---------------------------------------------------------------------------

// No mappedInput.update() here, and that omission is the whole of a bug that
// made this game feel frozen on the device.
//
// main.cpp:637 already calls gpio.update() once per frame, before any activity
// runs. InputManager::update() opens by clearing the one-shot touch edges
// (InputManager.cpp:388-392), and both wasTouchTap() and wasSwipe() gate on
// touchReleasedEvent. So a second update() here wiped the tap -- and the
// left-edge Back swipe, which is the only Back this board has -- microseconds
// after main.cpp latched it and microseconds before this function read it. A
// tap only survived if the 120ms release deadline happened to expire in the gap
// between the two calls, so most of them vanished with no feedback at all.
//
// Murdle was the only app in apps_local/ doing this; every other game reads the
// edges main.cpp sampled, which is why only this one was deaf.
void MurdleActivity::loop() {
  const bool backReleased = mappedInput.wasReleased(MappedInputManager::Button::Back);
  if (dirty && millis() - lastSaveAttempt >= 3000) {
    const bool wasFailed = saveFailed;
    flushSave();
    if (wasFailed != saveFailed) requestUpdate();
    if (!wasFailed && saveFailed) return;
  }
  if (saveNotice) {
    if (!interactionsReady && !backReleased) return;
    int x = 0, y = 0;
    const bool tapped = sidebuttons::poll(interactions, mappedInput, x, y, !(view == View::Cases));
    fui::ActionEvent event;
    if (tapped) {
      fui::InputSnapshot input;
      input.touchReleased = true;
      input.touchX = x;
      input.touchY = y;
      event = sidebuttons::route(interactions, input, mappedInput, *this, !(view == View::Cases));
    }
    if (backReleased || event.action == ui::ActionBack) {
      if (dirty) flushSave();
      shelf::leave(renderer, mappedInput);
      return;
    }
    if ((!interactions.hasButtonFocus() && mappedInput.wasReleased(MappedInputManager::Button::Confirm)) ||
        event.action == ui::ActionRetry || (tapped && sidebuttons::hasActivation(x, y))) {
      if (!dirty) {
        saveWritable = Storage.ready() && Storage.ensureDirectoryExists("/.crosspoint");
        const bool existing = Storage.exists(kSavePath) || Storage.exists(kSaveBackup);
        const bool loaded = saveWritable && loadState();
        if (loaded || (saveWritable && !existing)) {
          saveFailed = saveNotice = false;
          requestUpdate();
          return;
        }
        saveWritable = false;
      } else {
        if (Storage.ready()) saveWritable = Storage.ensureDirectoryExists("/.crosspoint");
        if (flushSave()) {
          requestUpdate();
          return;
        }
      }
      requestUpdate();
    }
    return;
  }
  if (generatePending) {
    // One pass later than the tap, so the frame saying so is already on the
    // panel before the work starts.
    generatePending = false;
    generateCase();
    requestUpdate();
    return;
  }

  // wasReleased, not wasPressed, and the difference is a whole screen: this
  // fired on the press edge and left to the folder, then the folder -- which
  // listens for the release like every other screen -- consumed the same
  // gesture's release and left again, so one Back from the Murdle menu landed
  // on Home. Every other game listens for the release; now this one does too.
  if (backReleased) {
    routeAction(ui::ActionBack, 0);
    return;
  }
  if (interactionsReady) {
    const auto event = sidebuttons::pageSwipe(interactions, mappedInput);
    if (event.action) {
      routeAction(event.action, event.value);
      return;
    }
  }
  if (view == View::Case && interactionsReady && interactions.routable()) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      routeAction(ui::ActionFace, (int(face) + direction + ui::kFaceCount) % ui::kFaceCount);
      return;
    }
    const auto swipe = mappedInput.wasSwipe();
    if (mappedInput.wasReleased(MappedInputManager::Button::Down) || swipe == MappedInputManager::SwipeDir::Up) {
      routeAction(ui::ActionPage, 1);
      return;
    }
    if (mappedInput.wasReleased(MappedInputManager::Button::Up) || swipe == MappedInputManager::SwipeDir::Down) {
      routeAction(ui::ActionPage, -1);
      return;
    }
  }

  if (view == View::HowTo && interactionsReady && interactions.routable()) {
    using Button = MappedInputManager::Button;
    if (mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Up)) {
      routeAction(ui::ActionPage, -1);
      return;
    }
    if (mappedInput.wasReleased(Button::Right) || mappedInput.wasReleased(Button::Down)) {
      routeAction(ui::ActionPage, 1);
      return;
    }
    if (mappedInput.wasReleased(Button::Confirm)) {
      routeAction(ui::ActionHowTo, 0);
      return;
    }
  }

  if (view == View::Cases && interactionsReady && interactions.routable()) {
    const int held = sidebuttons::longStep(mappedInput);
    if (const int direction = held ? held : sidebuttons::shortStep(mappedInput)) {
      routeAction(ui::ActionCampaignPage, direction);
      return;
    }
  }

  fui::InputSnapshot input{};
  int tapX = 0;
  int tapY = 0;
  if (sidebuttons::poll(interactions, mappedInput, tapX, tapY, !(view == View::Cases))) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  if (!input.touchReleased || !interactionsReady) return;

  // Asked BEFORE route(), because route() reports a tap it deliberately
  // suppressed as a default-constructed event -- the same thing it reports for
  // a tap that landed on nothing. The clue branch below reads "landed on
  // nothing" as "strike a clue", so without this a tap the panel could not
  // have shown would strike one, i.e. the reveal gate would CAUSE a wrong
  // action rather than prevent one. See RevealedInteractions.h.
  if (!interactions.routable()) return;

  const fui::ActionEvent hit = sidebuttons::route(interactions, input, mappedInput, *this, !(view == View::Cases));
  if (hit.action == ui::ActionGrid) {
    handleGridTap(tapX, tapY);
    return;
  }
  routeAction(static_cast<int>(hit.action), static_cast<int>(hit.value));
}

// A tap inside the grid, resolved against the layout that drew it. Never a
// second piece of arithmetic: the rule that has caught the most bugs in this
// fork is that a tappable region must be derived from the same values that
// placed the pixels.
// The grid and the clue list resolve a tap through gridLayout and the clue
// layout, both written by render(). Those are NOT hashed here: surfaceMeaning()
// is evaluated before render() runs, so reading them would mean stamping the
// previous frame's layout and testing the new one, i.e. comparing against a
// frame that was never on the panel. What DETERMINES those layouts is hashed
// instead, and loop() owns all of it.
//
// puzzle.seed is the case identity. generateCase() replaces the puzzle a pass
// after the tap that asked for it, which is the change here that does not
// belong to the finger on the glass.
uint32_t MurdleActivity::surfaceMeaning() const {
  uint32_t meaning = paintclock::mixMeaning(paintclock::kMeaningSeed, static_cast<uint32_t>(view));
  meaning = paintclock::mixMeaning(meaning, static_cast<uint32_t>(face));
  meaning = paintclock::mixMeaning(meaning, (hasCase && !solved) ? 1u : 0u);
  meaning = paintclock::mixMeaning(meaning, saveNotice ? 1u : 0u);
  meaning = paintclock::mixMeaning(meaning, view == View::Cases ? static_cast<uint32_t>(campaignPage) : 0u);
  meaning = paintclock::mixMeaning(meaning, view == View::Cases ? static_cast<uint32_t>(campaignTier) : 0u);
  meaning = paintclock::mixMeaning(meaning, uint32_t(face == ui::Face::Grid   ? gridPage
                                                     : face == ui::Face::Info ? infoCategory
                                                                              : page));
  meaning = paintclock::mixMeaning(meaning, showSolution ? 1u : 0u);
  return paintclock::mixMeaning(meaning, puzzle.seed);
}

void MurdleActivity::handleGridTap(const int x, const int y) {
  if (!hasCase || solved || showSolution || murdlesession::lives(wrongAccusations) == 0) return;
  // The grid registers as ONE rect, so route() has cleared the table but not
  // the cell: which cell this pixel is comes from gridLayout, and that is the
  // part nothing upstream gates.
  if (!surfaceRevealed()) return;
  ui::GridCell cell;
  if (!ui::cellAt(gridLayout, x, y, cell)) return;
  const int catA = cell.catA;
  const int catB = cell.catB;
  const int itemA = cell.itemA;
  const int itemB = cell.itemB;

  // One call, and it writes at most the cell that was tapped -- see Marks::tap.
  // When it writes nothing it says so, and that has to reach the screen: a tap
  // that changes nothing and says nothing is a tap that looks lost, on a panel
  // that takes a second to answer.
  const auto previous = marks.entered(catA, itemA, catB, itemB);
  lastCell = cell;
  const murdle::TapResult result = marks.tap(catA, itemA, catB, itemB);
  if (result.changed) {
    notice[0] = '\0';
    undoable = true;
    undoCell = cell;
    undoMark = previous;
    dirty = true;
  } else {
    murdletext::blockedLine(result, notice, sizeof(notice));
  }
  requestUpdate();
}

void MurdleActivity::submitAccusation() {
  if (!hasCase || solved || showSolution || murdlesession::lives(wrongAccusations) == 0) return;
  for (int c = 0; c < puzzle.shape.cats; ++c)
    if (picks[c] >= puzzle.shape.items) return;
  if (view != View::Accuse) return;
  const auto result = murdlesession::accuse(puzzle, picks, wrongAccusations, solved);
  if (result == murdlesession::Accusation::Incomplete || result == murdlesession::Accusation::Closed) return;
  const bool right = result == murdlesession::Accusation::Solved;
  if (right) {
    solved = true;
    ++solvedCount;
    if (campaignCase >= 0) campaign.finish(campaignCase);
    recordVerdict(wrongAccusations == 0 ? 1 : 2);
  } else {
    ++wrongCount;
  }
  view = View::Verdict;
  dirty = true;
  flushSave();
  flashOnNextPaint = true;
  requestUpdate();
}

void MurdleActivity::recordVerdict(const int outcome) { record = (record << 2) | static_cast<uint32_t>(outcome & 3); }

void MurdleActivity::routeAction(const int action, const int value) {
  // Anything that is not a grid tap has moved the player on, so whatever the
  // grid last refused has stopped being the thing in front of them.
  notice[0] = '\0';
  switch (action) {
    case ui::ActionPlay:
      openCase();
      break;
    case ui::ActionNewCase:
    case ui::ActionCampaign:
      view = View::Cases;
      campaignSelected = std::min(murdlecampaign::kCaseCount - 1, campaign.next());
      campaignTier = murdlecampaign::tier(campaignSelected);
      campaignPage = (campaignSelected - murdlecampaign::first(campaignTier)) / murdlecampaign::kCasesPerPage;
      requestUpdate();
      break;
    case ui::ActionCampaignPage:
      if (view != View::Cases) return;
      campaignPage = std::clamp(campaignPage + value, 0, murdlecampaign::kPagesPerTier - 1);
      campaignSelected = murdlecampaign::first(campaignTier) + campaignPage * murdlecampaign::kCasesPerPage;
      requestUpdate();
      break;
    case ui::ActionOpenCampaign:
      if (view != View::Cases) return;
      if (!campaign.unlocked(value)) return;
      if (value == campaignCase && hasCase) {
        if (solved) {
          resetCaseProgress();
          view = View::Case;
          flushSave();
          requestUpdate();
          return;
        }
        openCase();
        return;
      }
      pendingCampaign = value;
      requestNewCase();
      break;
    case ui::ActionConfirmNew:
      // The only path that skips the question, and it is the answer to it.
      solved = true;
      requestNewCase();
      break;
    case ui::ActionCancel:
      pendingCampaign = -1;
      view = View::Cases;
      requestUpdate();
      break;
    case ui::ActionResetProgress:
      if (view != View::Cases || !saveWritable) return;
      view = View::ConfirmReset;
      requestUpdate();
      break;
    case ui::ActionConfirmReset:
      if (view != View::ConfirmReset || !saveWritable) return;
      resetAllProgress();
      break;
    case ui::ActionSettings:
      view = View::Settings;
      requestUpdate();
      break;
    case ui::ActionBack:
      if (view == View::ConfirmReset) {
        view = View::Cases;
        requestUpdate();
        break;
      }
      if (view == View::Menu) {
        if (!flushSave()) {
          saveNotice = true;
          interactionsReady = false;
          requestUpdate();
          return;
        }
        shelf::leave(renderer, mappedInput);
        return;
      }
      if (view == View::Accuse) {
        view = View::Case;
        requestUpdate();
        break;
      }
      if (view == View::Case && (showSolution || solved || murdlesession::lives(wrongAccusations) == 0)) {
        showSolution = false;
        view = View::Verdict;
        requestUpdate();
        break;
      }
      goToMenu();
      break;
    case ui::ActionHowTo:
      if (view == View::HowTo) {
        if (howToPage + 1 < ui::howToPages(renderer.getScreenWidth() > renderer.getScreenHeight())) {
          ++howToPage;
          requestUpdate();
        }
      } else {
        view = View::HowTo;
        howToPage = 0;
        requestUpdate();
      }
      break;
    case ui::ActionFace:
      // Each face keeps its own page.
      face = value >= 0 && value < ui::kFaceCount ? static_cast<ui::Face>(value) : ui::Face::Clues;
      dirty = true;
      requestUpdate();
      break;
    case ui::ActionPage: {
      if (view == View::HowTo) {
        const int pages = ui::howToPages(renderer.getScreenWidth() > renderer.getScreenHeight());
        const int current = std::clamp(howToPage, 0, pages - 1);
        howToPage = std::clamp(current + value, 0, pages - 1);
        if (howToPage != current) requestUpdate();
        break;
      }
      // The top is clamped by the builder, which is the only thing that knows
      // how many pages the measured text came to. Here we only refuse to go
      // below zero and let the render report the real ceiling back.
      int& currentPage = face == ui::Face::Grid ? gridPage : face == ui::Face::Info ? infoCategory : page;
      int next = currentPage + value;
      if (next < 0) next = 0;
      if (next == currentPage) break;
      currentPage = next;
      dirty = true;
      requestUpdate();
      break;
    }
    case ui::ActionAccuse:
      if (!hasCase || solved || showSolution || murdlesession::lives(wrongAccusations) == 0) return;
      for (int c = 0; c < murdle::kMaxCats; ++c) picks[c] = ui::AccuseModel::kNothingPicked;
      view = View::Accuse;
      requestUpdate();
      break;
    case ui::ActionPick: {
      const int cat = value / 8;
      const int item = value % 8;
      if (view != View::Accuse || cat < 0 || cat >= puzzle.shape.cats || item >= puzzle.shape.items) break;
      picks[cat] = static_cast<uint8_t>(item);
      requestUpdate();
      break;
    }
    case ui::ActionConfirm:
      submitAccusation();
      break;
    case ui::ActionKeepLooking:
      if (!solved && murdlesession::lives(wrongAccusations) == 0) return;
      showSolution = false;
      view = View::Case;
      requestUpdate();
      break;
    case ui::ActionDone:
      if (solved && campaignCase >= 0 && campaign.next() < murdlecampaign::kCaseCount) {
        pendingCampaign = campaign.next();
        requestNewCase();
      } else
        goToMenu();
      break;
    case ui::ActionUndo:
      if (view == View::Case && undoable && !solved && !showSolution && murdlesession::lives(wrongAccusations) > 0) {
        marks.enter(undoCell.catA, undoCell.itemA, undoCell.catB, undoCell.itemB, undoMark);
        undoable = false;
        dirty = true;
        requestUpdate();
      }
      break;
    case ui::ActionClue:
      if (view == View::Case && face == ui::Face::Clues && !solved && !showSolution &&
          murdlesession::lives(wrongAccusations) > 0 && value >= 0 && value < puzzle.clueCount) {
        struck ^= 1u << value;
        dirty = true;
        requestUpdate();
      }
      break;
    case ui::ActionInfoCategory:
      if (value >= 0 && value < puzzle.shape.cats) {
        infoCategory = value;
        requestUpdate();
      }
      break;
    case ui::ActionReviewAccusation:
      if (!solved && murdlesession::lives(wrongAccusations) > 0) {
        view = View::Accuse;
        requestUpdate();
      }
      break;
    case ui::ActionRetryCase:
      if (view != View::Verdict || !hasCase || (!solved && murdlesession::lives(wrongAccusations) > 0)) return;
      tier = puzzle.tier;
      resetCaseProgress();
      view = View::Case;
      flushSave();
      requestUpdate();
      break;
    case ui::ActionSolution:
      if (!hasCase || murdlesession::lives(wrongAccusations) > 0) return;
      solutionMarks.reset(puzzle.shape);
      for (int a = 0; a < puzzle.shape.cats; ++a)
        for (int b = a + 1; b < puzzle.shape.cats; ++b)
          for (int row = 0; row < puzzle.shape.items; ++row)
            solutionMarks.enter(a, puzzle.assign[a][row], b, puzzle.assign[b][row], murdle::Mark::Yes);
      showSolution = true;
      face = ui::Face::Grid;
      gridPage = 0;
      view = View::Case;
      requestUpdate();
      break;
    case ui::ActionTier: {
      if (view != View::Cases || value < 0 || value >= murdle::kTierCount) return;
      const auto filter = static_cast<murdle::Tier>(value);
      if (campaignTier == filter) return;
      campaignTier = filter;
      campaignSelected = murdlecampaign::first(filter);
      campaignPage = 0;
      requestUpdate();
      break;
    }
    default:
      break;
  }
}

// ---------------------------------------------------------------------------

void MurdleActivity::render(RenderLock&&) {
  if (view == View::Cases) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  target.setFont(4, toybox::kLibraryFontId);
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  const auto device = target.deviceContext();
  toybox::Frame frame(target, device, noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  if (saveNotice) {
    ui::buildStorageError(screen, dirty);
    const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), tr(STR_RETRY), "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    interactionsReady = true;
    return;
  }
  gridLayout = ui::GridLayout{};

  switch (view) {
    case View::Cases:
      ui::buildCampaign(screen, campaign, campaignPage, campaignSelected, hasCase ? campaignCase : -1, campaignTier);
      break;
    case View::Case: {
      if (generatePending || !hasCase) {
        // The frame that has to be on the panel before generation starts.
        const fui::Rect body = screen.body();
        fui::TextStyle style;
        style.font = toybox::kDisplayFont;
        style.align = fui::TextAlign::Center;
        style.color = fui::Color::Black;
        target.text(fui::makeRect(body.x, static_cast<int16_t>(body.y + body.height / 2 - 30), body.width, 60),
                    tr(STR_UI_A_NEW_CASE_5332F5), style);
        break;
      }
      ui::CaseModel model;
      model.puzzle = &puzzle;
      model.marks = showSolution ? &solutionMarks : &marks;
      model.face = face;
      model.page = face == ui::Face::Grid ? gridPage : face == ui::Face::Info ? infoCategory : page;
      model.struck = struck;
      model.caseNumber = caseNumber;
      model.solved = solved || showSolution || murdlesession::lives(wrongAccusations) == 0;
      model.solution = showSolution;
      model.wrongAccusations = wrongAccusations;
      model.canUndo = undoable;
      model.lastA = lastCell.catA;
      model.lastB = lastCell.catB;
      model.lastIA = lastCell.itemA;
      model.lastIB = lastCell.itemB;
      model.notice = notice;
      const ui::CaseReport report = ui::buildCase(screen, model);
      gridLayout = report.grid;
      // Keep the page clamped by the builder that measured it.
      if (face == ui::Face::Clues)
        page = report.page;
      else if (face == ui::Face::Grid)
        gridPage = report.page;
      else
        infoCategory = report.page;
      break;
    }
    case View::Accuse: {
      ui::AccuseModel model;
      model.puzzle = &puzzle;
      for (int c = 0; c < murdle::kMaxCats; ++c) model.picks[c] = picks[c];
      model.wrongAccusations = wrongAccusations;
      ui::buildAccuse(screen, model);
      break;
    }
    case View::Verdict: {
      ui::VerdictModel model;
      model.puzzle = &puzzle;
      model.right = solved;
      model.nextCase = solved && campaignCase >= 0 && campaign.next() < murdlecampaign::kCaseCount;
      model.wrongAccusations = wrongAccusations;
      for (int c = 0; c < murdle::kMaxCats; ++c) model.picks[c] = picks[c];
      ui::buildVerdict(screen, model);
      break;
    }
    case View::Settings: {
      ui::SettingsModel model;
      model.tier = tier;
      model.caseOpen = hasCase && !solved;
      ui::buildSettings(screen, model);
      break;
    }
    case View::HowTo: {
      ui::HowToModel model;
      model.page = howToPage;
      ui::buildHowTo(screen, model);
      break;
    }
    case View::ConfirmNew:
      ui::buildConfirmNew(screen);
      break;
    case View::ConfirmReset:
      ui::buildConfirmReset(screen, campaign.count());
      break;
    case View::Menu:
    default: {
      ui::MenuModel model;
      model.hasCase = hasCase;
      model.caseSolved = solved;
      model.tier = tier;
      model.caseNumber = caseNumber;
      model.solvedCount = solvedCount;
      model.wrongCount = wrongCount;
      model.wrongAccusations = wrongAccusations;
      model.record = record;
      if (hasCase) {
        model.puzzle = &puzzle;
        model.marks = &marks;
        for (int i = 0; i < puzzle.clueCount; ++i) {
          if (struck & (1u << i)) ++model.cluesTicked;
        }
      }
      ui::buildMenu(screen, model);
      break;
    }
  }

  interactionsReady = true;
  toybox::reportOverflow(interactions, "Murdle", &renderer);

  const auto labels = mappedInput.mapLabels(tr(STR_QUEST_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  // A new case and a verdict are page turns; a mark is not. Spend the full
  // refresh on meaning rather than on frames.
  renderer.displayBuffer(flashOnNextPaint ? HalDisplay::FULL_REFRESH : HalDisplay::FAST_REFRESH);
  flashOnNextPaint = false;
}

// ---------------------------------------------------------------------------

bool MurdleActivity::flushSave() {
  if (!dirty) return true;
  lastSaveAttempt = millis();
  if (!saveState()) {
    if (!saveFailed) {
      saveNotice = true;
      interactionsReady = false;
    }
    saveFailed = true;
    LOG_ERR("MRDL", "Save failed; previous record retained");
    return false;
  }
  dirty = saveFailed = saveNotice = false;
  return true;
}

bool MurdleActivity::saveState() {
  if (!saveWritable || !Storage.ready()) return false;
  SaveState state{};
  state.campaignCase = campaignCase >= 0 ? campaignCase : murdlesave::kNoCampaign;
  std::memcpy(state.completed, campaign.completed, sizeof(state.completed));
  state.tier = static_cast<uint8_t>(tier);
  state.caseTier = static_cast<uint8_t>(puzzle.tier);
  state.hasCase = hasCase ? 1 : 0;
  state.solved = solved ? 1 : 0;
  state.seed = seed;
  state.peopleSeed = hasCase ? puzzle.peopleSeed : 0;
  state.fingerprint = hasCase ? murdle::fingerprint(puzzle) : 0;
  state.struck = struck;
  state.record = record;
  state.caseNumber = static_cast<uint16_t>(caseNumber);
  state.solvedCount = static_cast<uint16_t>(solvedCount);
  state.wrongCount = static_cast<uint16_t>(wrongCount);
  state.wrongAccusations = static_cast<uint8_t>(wrongAccusations);
  state.face = static_cast<uint8_t>(face);
  state.page = static_cast<uint8_t>(page);
  if (hasCase) packMarks(marks, puzzle.shape, state.marks, sizeof(state.marks));

  uint8_t bytes[murdlesave::kBytes];
  murdlesave::encode(state, bytes);
  {
    HalFile file;
    if (!Storage.openFileForWrite("MRDL", kSavePending, file) || file.write(bytes, sizeof(bytes)) != sizeof(bytes))
      return false;
    file.flush();
  }
  if (!readSaveRecord(kSavePending, state) || std::memcmp(&state, bytes + 1, sizeof(state))) return false;
  if (Storage.exists(kSavePath)) {
    bool future = false;
    const bool valid = readSaveRecord(kSavePath, state, &future);
    if (future) {
      saveWritable = false;
      return false;
    }
    if (valid) {
      if (Storage.exists(kSaveBackup) && !Storage.remove(kSaveBackup)) return false;
      if (!Storage.rename(kSavePath, kSaveBackup)) return false;
    } else if (!Storage.remove(kSavePath))
      return false;
  }
  if (!Storage.rename(kSavePending, kSavePath)) return false;
  if (Storage.exists(kSaveBackup)) Storage.remove(kSaveBackup);
  return true;
}

bool MurdleActivity::loadState() {
  SaveState state{};
  bool future = false;
  const bool current = readSaveRecord(kSavePath, state, &future);
  if (future) {
    saveWritable = false;
    return false;
  }
  if (!current && !readSaveRecord(kSaveBackup, state, &future)) return false;
  if (future) {
    saveWritable = false;
    return false;
  }
  std::memcpy(campaign.completed, state.completed, sizeof(state.completed));
  campaignCase = state.campaignCase < murdlecampaign::kCaseCount ? state.campaignCase : -1;
  if (campaignCase >= 0 && (state.seed != murdlecampaign::kSeeds[campaignCase] ||
                            state.caseTier != uint8_t(murdlecampaign::tier(campaignCase))))
    campaignCase = -1;

  tier = state.tier < murdle::kTierCount ? static_cast<murdle::Tier>(state.tier) : murdle::Tier::Elementary;
  record = state.record;
  caseNumber = state.caseNumber;
  solvedCount = state.solvedCount;
  wrongCount = state.wrongCount;
  hasCase = false;
  if (state.hasCase == 0) return true;

  if (!scratch) scratch = makeUniqueNoThrow<murdle::Scratch>();
  if (!scratch) {
    LOG_ERR("MRDL", "OOM restoring a case; starting without one");
    return true;
  }
  const murdle::Tier caseTier = state.caseTier < murdle::kTierCount ? static_cast<murdle::Tier>(state.caseTier) : tier;
  const murdle::Shape shape = murdle::shapeOf(caseTier);
  uint8_t cast[murdle::kMaxCats][murdle::kMaxItems];
  const bool drawn = murdle::drawCast(state.seed, shape, cast);
  const bool built =
      drawn && murdle::generate(caseTier, state.seed, cast, murdle::attrMasksFor(cast, shape), *scratch, puzzle);
  scratch.reset();

  // The fingerprint is the whole point of storing a seed instead of a puzzle.
  // A generator that has changed since this save was written builds a different
  // case from the same seed, and restoring somebody's marks onto it would be
  // silently wrong in a way nothing else could catch.
  if (!built || murdle::fingerprint(puzzle) != state.fingerprint) {
    LOG_INF("MRDL", "Saved case no longer rebuilds; dropping it");
    return true;
  }

  puzzle.peopleSeed = state.peopleSeed;
  lastCell.catA = -1;
  showSolution = undoable = false;
  unpackMarks(state.marks, sizeof(state.marks), puzzle.shape, marks);
  hasCase = true;
  solved = state.solved != 0;
  seed = state.seed;
  struck = state.struck;
  wrongAccusations = state.wrongAccusations;
  face = state.face < ui::kFaceCount ? static_cast<ui::Face>(state.face) : ui::Face::Clues;
  page = state.page;
  return true;
}
