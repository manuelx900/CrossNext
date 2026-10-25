#include "DraughtsActivity.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include "../Shelf.h"
#include "../ui/PixelMenuTheme.h"
#include "components/SideButtonNavigation.h"

namespace {
constexpr char kConfig[] = "/.crosspoint/draughts.cfg";
}
std::unique_ptr<Activity> DraughtsActivity::create(GfxRenderer& r, MappedInputManager& i) {
  auto result = makeUniqueNoThrow<DraughtsActivity>(r, i);
  if (!result) LOG_ERR("DRAUGHTS", "OOM: %u bytes", unsigned(sizeof(DraughtsActivity)));
  return result;
}
void DraughtsActivity::onEnter() {
  Activity::onEnter();
  toybox::ensureFonts(renderer);
  random = millis() | 1;
  HalFile file;
  if (Storage.exists(kConfig) && Storage.openFileForRead("DRAUGHTS", kConfig, file)) {
    uint8_t bytes[8]{};
    if (file.size() == 8 && file.read(bytes, 8) == 8 && bytes[0] == 'D' && bytes[1] == 'R') {
      readOnly = bytes[2] > 2;
      const bool legacy = bytes[2] == 1;
      const uint8_t colour = legacy ? 0 : bytes[6];
      if ((legacy || bytes[2] == 2) && bytes[3] < 4 && bytes[4] < 3 && bytes[5] < 2 && colour < 2 &&
          bytes[7] == uint8_t(bytes[3] ^ bytes[4] ^ bytes[5] ^ colour ^ 0xa5)) {
        rules = draughts::Rules(bytes[3]);
        difficulty = bytes[4];
        local = bytes[5];
        humanBlack = colour;
      }
    }
  }
  requestUpdate();
}
void DraughtsActivity::save() {
  if (!dirty || readOnly) return;
  uint8_t bytes[8] = {'D',
                      'R',
                      2,
                      uint8_t(rules),
                      uint8_t(difficulty),
                      uint8_t(local),
                      uint8_t(humanBlack),
                      uint8_t(uint8_t(rules) ^ difficulty ^ local ^ humanBlack ^ 0xa5)};
  HalFile file;
  constexpr char pending[] = "/.crosspoint/draughts.cfg.part";
  saveFailed = !Storage.openFileForWrite("DRAUGHTS", pending, file) || file.write(bytes, 8) != 8;
  if (!saveFailed) {
    file.flush();
    file.close();
    saveFailed = !Storage.replaceFile(pending, kConfig);
  }
  if (saveFailed)
    LOG_ERR("DRAUGHTS", "Could not save settings");
  else
    dirty = false;
}
void DraughtsActivity::onExit() {
  save();
  Activity::onExit();
}
void DraughtsActivity::begin() {
  draughts::start(game, rules);
  gameLocal = local;
  gameHumanBlack = humanBlack;
  gameDifficulty = difficulty;
  active = true;
  selected = -1;
  result = draughtsui::Outcome::Playing;
  legal.begin(game);
  view = draughtsui::View::Board;
  ready = false;
  requestUpdate();
}
void DraughtsActivity::action(freeink::ui::ActionId event, int value) {
  using namespace draughtsui;
  switch (event) {
    case Play:
      if (active && result == Outcome::Playing)
        view = View::Board;
      else {
        begin();
        return;
      }
      break;
    case draughtsui::Settings:
      pendingRules = rules;
      pendingDifficulty = difficulty;
      pendingLocal = local;
      pendingHumanBlack = humanBlack;
      view = View::Settings;
      break;
    case Help:
      helpPage = 0;
      view = View::Help;
      break;
    case Rule:
      if (value >= 0 && value < 4) {
        pendingRules = draughts::Rules(value);
      }
      break;
    case Difficulty:
      if (value >= 0 && value < 3) {
        pendingDifficulty = value;
      }
      break;
    case Local:
      pendingLocal = value != 0;
      break;
    case Colour:
      if (view != View::Settings || value < 0 || value > 1) return;
      pendingHumanBlack = value != 0;
      break;
    case Menu:
      if (view == View::Settings) commitSettings();
      view = View::Menu;
      break;
    case Review:
      if (result == Outcome::Playing) return;
      view = View::Review;
      break;
    case Result:
      if (result == Outcome::Playing) return;
      view = View::Result;
      break;
    case NewGame:
      if (active && (view == View::Board || view == View::Menu) && !game.draw && legal.count()) {
        newGameReturnView = view;
        view = View::ConfirmNew;
      } else {
        begin();
        return;
      }
      break;
    case HelpPage: {
      const int next = std::clamp(helpPage + value, 0, renderer.getScreenWidth() > renderer.getScreenHeight() ? 7 : 3);
      if (next == helpPage) return;
      helpPage = next;
    } break;
    case HelpDone:
      view = View::Menu;
      break;
    case Cancel:
      view = newGameReturnView;
      break;
    default:
      return;
  }
  ready = false;
  requestUpdate();
}
void DraughtsActivity::commitSettings() {
  if (rules != pendingRules || difficulty != pendingDifficulty || local != pendingLocal ||
      humanBlack != pendingHumanBlack) {
    rules = pendingRules;
    difficulty = pendingDifficulty;
    local = pendingLocal;
    humanBlack = pendingHumanBlack;
    dirty = true;
  }
  save();
}
uint32_t DraughtsActivity::surfaceMeaning() const {
  uint32_t value = paintclock::mixMeaning(paintclock::kMeaningSeed, uint32_t(view));
  value = paintclock::mixMeaning(value,
                                 uint32_t(game.turn) | (uint32_t(game.size) << 8) | (uint32_t(game.forced + 1) << 16));
  return paintclock::mixMeaning(value,
                                legal.ready() && (gameLocal || game.turn == int(gameHumanBlack)) && !game.draw ? 1 : 0);
}
void DraughtsActivity::loop() {
  RenderLock lock(*this);
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    if (view == draughtsui::View::Menu) {
      shelf::leave(renderer, mappedInput);
      return;
    }
    if (view == draughtsui::View::Settings) commitSettings();
    save();
    view = view == draughtsui::View::Review ? draughtsui::View::Result : draughtsui::View::Menu;
    ready = false;
    requestUpdate();
    return;
  }
  if (active && !legal.ready()) {
    if (legal.tick()) {
      ready = false;
      requestUpdate();
    }
    return;
  }
  if (active && result == draughtsui::Outcome::Playing) {
    const auto terminal = draughtsui::outcomeFor(legal.ready(), legal.count(), game.draw, game.turn);
    if (terminal != draughtsui::Outcome::Playing) {
      result = terminal;
      selected = -1;
      if (view == draughtsui::View::Board) view = draughtsui::View::Result;
      ready = false;
      requestUpdate();
      return;
    }
  }
  if (view == draughtsui::View::Board && ready && surfaceRevealed() && !gameLocal && game.turn != int(gameHumanBlack) &&
      !game.draw) {
    const int index = draughts::choose(game, legal, gameDifficulty, random);
    if (index >= 0) {
      const auto move = legal.at(index).move;
      draughts::play(game, legal, move.from, move.to);
      selected = -1;
      legal.begin(game);
      ready = false;
      requestUpdate();
    }
    return;
  }
  if (ready && view == draughtsui::View::Help) {
    if (const int direction = sidebuttons::shortStep(mappedInput)) {
      action(draughtsui::HelpPage, direction);
      return;
    }
  }
  int x, y;
  if (!ready || !sidebuttons::poll(interactions, mappedInput, x, y, !(view == draughtsui::View::Settings))) return;
  if (draughtsui::canPlay(view, surfaceRevealed(), legal.ready(), gameLocal, game.turn, game.draw, legal.count(),
                          gameHumanBlack)) {
    const int square =
        draughtsui::squareAt(toybox::makeTarget(renderer).deviceContext(), game.size, x, y, gameHumanBlack);
    if (square >= 0) {
      if (selected >= 0 && draughts::play(game, legal, selected, square)) {
        selected = game.forced;
        legal.begin(game);
      } else if (draughts::owner(game.cells[square]) == game.turn && !(game.cells[square] & draughts::kTaken))
        selected = square;
      ready = false;
      requestUpdate();
      return;
    }
  }
  freeink::ui::InputSnapshot input;
  input.touchReleased = true;
  input.touchX = x;
  input.touchY = y;
  const auto event = sidebuttons::route(interactions, input, mappedInput, *this, !(view == draughtsui::View::Settings));
  action(event.action, event.value);
}
void DraughtsActivity::render(RenderLock&&) {
  if (view == draughtsui::View::Settings) interactions.setFocusedIndex(-1);
  renderer.clearScreen();
  auto target = toybox::makeTarget(renderer);
  pixelui::bindFonts(target, renderer);
  const freeink::ui::InputSnapshot noInput{};
  toybox::Frame frame(target, target.deviceContext(), noInput, interactions, nullptr, toybox::headerStatus(renderer));
  toybox::Screen screen(frame);
  draughtsui::build(screen, {&game, &legal, view, view == draughtsui::View::Settings ? pendingRules : rules,
                             view == draughtsui::View::Settings         ? pendingDifficulty
                             : view == draughtsui::View::Menu && active ? gameDifficulty
                                                                        : difficulty,
                             selected,
                             view == draughtsui::View::Settings ? pendingLocal
                             : active                           ? gameLocal
                                                                : local,
                             active, saveFailed, helpPage, result,
                             view == draughtsui::View::Settings ? pendingHumanBlack
                             : active                           ? gameHumanBlack
                                                                : humanBlack});
  toybox::reportOverflow(interactions, "Draughts", &renderer);
  ready = true;
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
