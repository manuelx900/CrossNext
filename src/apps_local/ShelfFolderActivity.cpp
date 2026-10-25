#include "ShelfFolderActivity.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "Shelf.h"
#include "ShelfScreen.h"
#include "ui/Toybox.h"
#include "ui/ToyboxFonts.h"
#include "ui/ToyboxTheme.h"

std::unique_ptr<Activity> ShelfFolderActivity::create(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                      const int folderIndex) {
  return makeUniqueNoThrow<ShelfFolderActivity>(renderer, mappedInput, folderIndex);
}

int ShelfFolderActivity::rowsInList() const {
  return choosing ? shelf::folders()[folder].count : shelf::shownCount(folder);
}

int ShelfFolderActivity::itemAtRow(const int row) const {
  if (row < 0) return -1;
  // While choosing, the list IS the registry: every item is on screen, in
  // registry order, so the row is the item. That identity is the reason the
  // chooser lists everything -- a chooser over the shown items could not show
  // you what you had hidden.
  if (choosing) return row < shelf::folders()[folder].count ? row : -1;
  return shelf::shownItem(folder, row);
}

void ShelfFolderActivity::buildPage(const int first, const int count) {
  const shelf::Folder& self = shelf::folders()[folder];
  // Cannot happen on this panel: the tallest band a folder can have fits ten
  // rows and the array holds sixteen. Logged rather than clamped in silence
  // because a page that quietly drew fewer rows than it was asked for is
  // exactly the failure the registry cap used to have.
  if (count > kMaxRowsPerPage) LOG_ERR("SHELF", "Page of %d exceeds %d rows", count, kMaxRowsPerPage);
  for (int i = 0; i < count && i < kMaxRowsPerPage; ++i) {
    const int item = itemAtRow(first + i);
    if (item < 0) {
      // A row the list does not have. Cannot happen -- render() slices against
      // the same count -- and is logged rather than drawn blank, because a row
      // with no label is the one thing a person cannot report.
      LOG_ERR("SHELF", "Row %d has no item in %s", first + i, self.title);
      items[i].label = "";
      icons[i] = nullptr;
      checks[i] = false;
      // And it must not still answer for whatever the last page put here: the
      // row is registered whatever its label says, so a stale value is a blank
      // row that opens a game.
      items[i].actionValue = -1;
      continue;
    }
    items[i].label = shelf::label(self.items[item].title);
    icons[i] = self.items[item].icon;
    checks[i] = !shelf::isHidden(folder, item);
    // The row's index in the WHOLE list, so a tap says which entry it is rather
    // than which row of which page. The list component emits actionValue rather
    // than the row, which is what lets the screen be handed a slice at all.
    items[i].actionValue = static_cast<int16_t>(first + i);
  }
}

void ShelfFolderActivity::onEnter() {
  Activity::onEnter();
  navigation.begin(mappedInput.isPressed(MappedInputManager::Button::NavNext) ||
                   mappedInput.isPressed(MappedInputManager::Button::NavPrevious));
  toybox::ensureFonts(renderer);
  itemCount = rowsInList();
  selected = 0;
  buttonCursorVisible = false;
  // The page itself is built in render(), which is the only place that knows how
  // many rows fit.
  requestUpdate();
}

void ShelfFolderActivity::setChoosing(const bool on) {
  if (choosing == on) return;
  // The table on the panel belongs to the mode that drew it, and every action
  // id on this screen changes meaning here: the row that opened CHESS a moment
  // ago now hides it. render() runs on the other core and only after this
  // loop() returns, so between the chip's tap and the repaint there is a window
  // where the OLD table is live and `choosing` is already the new value -- and
  // an unchanged table always routes, by design, which is what keeps touch
  // alive everywhere else. Disarmed here rather than left to the rebuild,
  // because the rebuild is the thing that has not happened yet.
  interactionsReady = false;
  // The item under the finger before the switch, kept through it. The two lists
  // are different lengths, so a row carried across straight would be a
  // different game -- and the page bar would say the same number either way.
  const int item = itemAtRow(selected);
  choosing = on;
  itemCount = rowsInList();
  if (choosing) {
    // The chooser's rows ARE the registry, so the item is the row.
    selected = item < 0 ? 0 : item;
    requestUpdate();
    return;
  }
  // Coming back out, the item may be the one that was just hidden, and then
  // there is no row for it: shownRowFor answers with the nearest surviving one
  // and resumeRowFor pins that to the end of a list that shrank underneath.
  selected = shelfui::resumeRowFor(shelf::shownRowFor(folder, item < 0 ? 0 : item), itemCount);
  requestUpdate();
}

bool ShelfFolderActivity::showPage(const int page) {
  const int landing = shelfui::rowForPage(page, rowsPerPage);
  // Which page a step actually landed on, because "it moved by two" and "it
  // started somewhere else" look identical from outside and cost three cold
  // testers an evening between them.
  LOG_DBG("SHELF", "Page %d -> %d of %d (row %d of %d)", shelfui::pageFor(selected, rowsPerPage), page,
          shelfui::pageCountFor(itemCount, rowsPerPage), landing, itemCount);
  if (landing < 0 || landing >= itemCount || landing == selected) return false;
  // Within this screen the page is DERIVED from the selection and held nowhere
  // else, so changing page means moving the selection onto the page's first row.
  selected = landing;
  requestUpdate();
  return true;
}

void ShelfFolderActivity::loop() {
  namespace fui = freeink::ui;
  if (!navigation.capture(mappedInput, millis())) LOG_ERR("SHELF", "Navigation queue full");
  RenderLock lock(RenderLock::Mode::Try);
  if (!lock.ownsLock()) return;

  const auto swipe = mappedInput.wasSwipe();
  int touchX = 0, touchY = 0;
  if (buttonCursorVisible &&
      (swipe != MappedInputManager::SwipeDir::None || mappedInput.wasScreenTapped(touchX, touchY))) {
    buttonCursorVisible = false;
    requestUpdate();
  }
  const bool swipeBack = swipe == MappedInputManager::SwipeDir::Right && shelfui::pageFor(selected, rowsPerPage) == 0;
  if (swipeBack) {
    // Back has two rules in this fork -- an app returns to its folder, a folder
    // returns to Home -- and choosing adds the only exception: it closes the
    // mode and stays put. A mode that Back walked straight out of would make
    // Back mean "undo the thing I was doing" on the one screen where it is also
    // the only way out of the app, and the taps are already saved by then.
    if (choosing) {
      setChoosing(false);
      return;
    }
    // A folder's parent is Home. It does not name Home; leave() knows.
    shelf::leave(renderer, mappedInput);
    return;
  }

  fui::InputSnapshot input;
  int tapX = 0;
  int tapY = 0;
  if (mappedInput.wasScreenTapped(tapX, tapY)) {
    input.touchReleased = true;
    input.touchX = static_cast<int16_t>(tapX);
    input.touchY = static_cast<int16_t>(tapY);
  }
  sectionnav::Input::Event command;
  bool moved = false;
  while (navigation.pop(command)) {
    using Kind = sectionnav::Input::Kind;
    if (command.kind == Kind::Back) {
      navigation.clear();
      if (choosing)
        setChoosing(false);
      else
        shelf::leave(renderer, mappedInput);
      return;
    }
    if (command.kind == Kind::Confirm) {
      if (!buttonCursorVisible) continue;
      const int item = itemAtRow(selected);
      if (item >= 0) {
        navigation.clear();
        if (choosing) {
          shelf::setHidden(folder, item, !shelf::isHidden(folder, item));
          requestUpdate();
        } else
          shelf::openItem(folder, item, renderer, mappedInput);
        return;
      }
      continue;
    }
    if (!itemCount) continue;
    if (command.kind == Kind::Section || command.kind == Kind::Step) {
      const int target = std::clamp(shelfui::pageFor(selected, rowsPerPage) + command.amount, 0,
                                    shelfui::pageCountFor(itemCount, rowsPerPage) - 1);
      moved |= showPage(target);
    }
  }
  if (moved) {
    requestUpdate();
    return;
  }
  // The folder owns horizontal swipes: right goes Back on the first page,
  // and pages backward elsewhere, including from the left edge.
  const bool next = swipe == MappedInputManager::SwipeDir::Left;
  const bool prev = swipe == MappedInputManager::SwipeDir::Right;

  if (itemCount > 0 && (next || prev)) {
    // A folder that fits on one page has no page to step to, and moving the
    // resumed row to the top instead would be a step that changed something
    // without going anywhere.
    const int pages = shelfui::pageCountFor(itemCount, rowsPerPage);
    if (pages > 1) {
      const int from = shelfui::pageFor(selected, rowsPerPage);
      const int to = shelfui::pageStepClamped(from, pages, next ? 1 : -1);
      // Nothing happens at the ends, and nothing repaints either: a full-panel
      // refresh that redraws the same eight rows is half a second of blink
      // saying a step was taken when none was. What says so instead is the page
      // bar, which reads 3 of 1 2 3.
      if (to != from) {
        showPage(to);
      } else {
        LOG_DBG("SHELF", "Page %d of %d is an end; step refused", from, pages);
      }
    }
    return;
  }

  if (!input.touchReleased || !interactionsReady) return;

  const fui::ActionEvent event = interactions.route(input);
  if (event.action == shelfui::ActionOpen) {
    selected = event.value;
    const int item = itemAtRow(selected);
    if (item >= 0) shelf::openItem(folder, item, renderer, mappedInput);
    return;
  }
  if (event.action == shelfui::ActionChoose) {
    setChoosing(!choosing);
    return;
  }
  if (event.action == shelfui::ActionToggleShown) {
    const int item = itemAtRow(event.value);
    if (item >= 0) {
      shelf::setHidden(folder, item, !shelf::isHidden(folder, item));
      // The row the finger is on stays the row the finger is on: the chooser's
      // list is the registry, so hiding something never moves it. The list that
      // shrinks is the other one, and it is not on screen.
      selected = event.value;
      requestUpdate();
    }
    return;
  }
  if (event.action == shelfui::ActionGoToPage) {
    showPage(event.value);
    return;
  }
}

void ShelfFolderActivity::render(RenderLock&&) {
  namespace fui = freeink::ui;

  const shelf::Folder& self = shelf::folders()[folder];

  renderer.clearScreen();
  fui::GfxRendererTarget target = toybox::makeTarget(renderer);
  const fui::DeviceContext device = target.deviceContext();
  const fui::InputSnapshot noInput{};
  interactionsReady = false;
  toybox::Frame frame(target, device, noInput, interactions);
  toybox::Screen screen(frame);

  // Keep the selection on screen. The list is virtualized, so a selection below
  // the fold would otherwise be styled on a row that is never drawn.
  //
  // Derive the page from the current row so button and touch navigation agree.
  const shelfui::Paging paging = shelfui::pagingFor(device, itemCount);
  rowsPerPage = paging.rowsPerPage;
  const int page = shelfui::pageFor(selected, rowsPerPage);
  const int first = shelfui::rowForPage(page, rowsPerPage);
  // Short on the last page, which is the whole reason the screen is handed a
  // slice rather than the folder plus an offset. See MenuModel::items.
  const int onThisPage = itemCount - first < rowsPerPage ? itemCount - first : rowsPerPage;
  buildPage(first, onThisPage);

  // Toybox chrome is capitals; upstream's Home list is Title Case. The registry
  // stores the Title Case name because that is the one a person reads on Home,
  // and the shout happens here, where our own design language starts.
  char shouted[24];
  size_t n = 0;
  for (const char* c = shelf::label(self.title); *c != '\0' && n + 1 < sizeof(shouted); ++c, ++n) {
    shouted[n] = *c >= 'a' && *c <= 'z' ? static_cast<char>(*c - 'a' + 'A') : *c;
  }
  shouted[n] = '\0';

  shelfui::MenuModel model;
  model.title = shouted;
  model.mark = self.mark;
  model.items = items;
  model.icons = icons;
  model.count = onThisPage;
  // Non-null is the mode, for the screen as well as for this file.
  model.checks = choosing ? checks : nullptr;
  model.selected = buttonCursorVisible ? selected - first : -1;
  model.page = page;
  model.pageCount = paging.pageCount;
  shelfui::buildMenu(screen, model);

  interactionsReady = true;
  toybox::reportOverflow(interactions, self.title);

  // Only a board with no touch panel ever draws these (BaseTheme::drawButtonHints
  // returns immediately when gpio.hasTouch()), so they are the Sticky's copy of
  // what the chip in the corner says.
  const auto labels =
      choosing ? mappedInput.mapLabels(tr(STR_DONE), tr(STR_UI_SHOW_HIDE_06C236), tr(STR_2048_UP), tr(STR_2048_DOWN))
               : mappedInput.mapLabels(tr(STR_QUEST_BACK), tr(STR_OPEN), tr(STR_2048_UP), tr(STR_2048_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
