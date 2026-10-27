#include "ChessScreens.h"

#include <I18n.h>

#include "../ui/CompactArt.h"
#include "../ui/Dashboard.h"
#include "../ui/GameFrontDoor.h"
#include "../ui/MatchResult.h"
#include "../ui/ResultArt.h"
#include "ChessCore.h"
#include "ChessStyle.h"

namespace chessui {

namespace {

// Legacy row routing excludes undo and keeps actions separate from preferences.
constexpr MenuRow kBoardVsComputer[] = {MenuRow::NewGame, MenuRow::Opponent, MenuRow::Level, MenuRow::PlayAs,
                                        MenuRow::Hints};
// Both human modes show the same rows: level and colour mean nothing when the
// other player is a person in front of you.
constexpr MenuRow kBoardVsPerson[] = {MenuRow::NewGame, MenuRow::Opponent, MenuRow::Hints};
constexpr MenuRow kMenuVsComputer[] = {MenuRow::Opponent, MenuRow::Level, MenuRow::PlayAs, MenuRow::Hints};
constexpr MenuRow kMenuVsPerson[] = {MenuRow::Opponent, MenuRow::Hints};

void caps(toybox::Screen& screen, fui::Rect box, const char* text, int font = 0, int lines = 1,
          fui::TextAlign align = fui::TextAlign::Left) {
  char capital[160];
  pixelui::copyCaps(capital, sizeof(capital), text);
  fittext::draw(screen.target(), box, capital, font, lines, align);
}

struct RowSet {
  const MenuRow* rows;
  int count;
};

RowSet rowSet(const SettingsModel& model) {
  const bool computer = model.opponent == Opponent::Computer;
  if (model.fromMenu) {
    if (computer) return {kMenuVsComputer, static_cast<int>(sizeof(kMenuVsComputer) / sizeof(kMenuVsComputer[0]))};
    return {kMenuVsPerson, static_cast<int>(sizeof(kMenuVsPerson) / sizeof(kMenuVsPerson[0]))};
  }
  if (computer) return {kBoardVsComputer, static_cast<int>(sizeof(kBoardVsComputer) / sizeof(kBoardVsComputer[0]))};
  return {kBoardVsPerson, static_cast<int>(sizeof(kBoardVsPerson) / sizeof(kBoardVsPerson[0]))};
}

}  // namespace

int visibleRows(const SettingsModel& model) { return rowSet(model).count; }

MenuRow rowAt(const SettingsModel& model, const int visibleIndex) {
  const RowSet set = rowSet(model);
  const int clamped = visibleIndex < 0 ? 0 : (visibleIndex >= set.count ? set.count - 1 : visibleIndex);
  return set.rows[clamped];
}

const char* rowLabel(const MenuRow row) {
  switch (row) {
    case MenuRow::NewGame:
      return tr(STR_UI_NEW_GAME_F6F041);
    case MenuRow::TakeBack:
      return tr(STR_UI_TAKE_BACK_A026F7);
    case MenuRow::Level:
      return tr(STR_UI_LEVEL_F481CC);
    case MenuRow::PlayAs:
      return tr(STR_UI_PLAY_AS_7859F5);
    case MenuRow::Opponent:
      return tr(STR_UI_OPPONENT_974974);
    case MenuRow::Hints:
      return tr(STR_UI_MOVE_HINTS_3D49F5);
    default:
      return "";
  }
}

const char* rowValue(const SettingsModel& model, const MenuRow row) {
  switch (row) {
    case MenuRow::TakeBack:
      return model.canTakeBack ? "" : tr(STR_UI_NONE_0654A0);
    case MenuRow::Level:
      return model.level == 0 ? tr(STR_UI_EASY_493F11) : (model.level == 1 ? "NORMAL" : tr(STR_UI_HARD_A06A58));
    case MenuRow::PlayAs:
      return model.humanPlaysWhite ? tr(STR_UI_WHITE_40D23F) : tr(STR_UI_BLACK_A9414A);
    case MenuRow::Opponent:
      switch (model.opponent) {
        case Opponent::Computer:
          return tr(STR_UI_COMPUTER_9E47DA);
        case Opponent::PassAndPlay:
          return tr(STR_CHESS_PERSON);
        case Opponent::FaceToFace:
          return tr(STR_UI_F2F_087904);
      }
      return "";
    case MenuRow::Hints:
      return model.showHints ? tr(STR_UI_ON_387D7A) : tr(STR_UI_OFF_AD5048);
    default:
      return "";
  }
}

void buildSettings(toybox::Screen& screen, const SettingsModel& model) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = tr(STR_SETTINGS_TITLE);
  header.back = true;
  pixelui::header(screen, header);
  backui::publish(true, pixelui::geometry(screen.device()).back);
  const auto body = screen.body();
  const bool wide = body.width > body.height;
  const int heroH = wide ? 60 : 78;
  compactart::draw(screen, fui::makeRect(body.x + 8, body.y + 2, 64, heroH - 12), compactart::ChessKnight);
  pixelui::text(screen.target(), fui::makeRect(body.x + 88, body.y, wide ? 176 : body.width - 88, wide ? 40 : 44),
                tr(STR_UI_CHESS_902609), pixelui::kTitle);
  pixelui::text(screen.target(),
                fui::makeRect(body.x + (wide ? 280 : 88), body.y + (wide ? 4 : 44), body.width - (wide ? 280 : 88), 36),
                tr(STR_CHESS_CONFIGURE), pixelui::kSmall);
  const int gap = 18, cols = wide ? 2 : 1, width = (body.width - (cols - 1) * gap) / cols, step = 104;
  const auto group = [&](int n, const char* label) {
    const int col = wide ? n % 2 : 0, row = wide ? n / 2 : n;
    const auto r = fui::makeRect(body.x + col * (width + gap), body.y + heroH + row * step, width, step - 8);
    screen.target().fill(fui::makeRect(r.x, r.y, r.width, 2), dashboard::ink);
    pixelui::text(screen.target(), fui::makeRect(r.x, r.y + 8, r.width, 30), label, pixelui::kBody);
    return fui::makeRect(r.x, r.y + 38, r.width, wide ? 50 : 48);
  };
  const auto choice = [&](fui::Rect band, int n, int count, const char* label, SettingValue value, bool active,
                          const fui::BitmapRef* art = nullptr) {
    const auto r = fui::makeRect(band.x + band.width * n / count, band.y,
                                 band.width * (n + 1) / count - band.width * n / count, band.height);
    active = screen.frame().buttonSelected(ActionSettingValue, int(value), active);
    dashboard::button(screen, r, art ? "" : label, ActionSettingValue, int(value), active);
    if (art) {
      screen.target().bitmap(fui::makeRect(r.x + 20, r.y + 10, 36, r.height - 20), *art, fui::BitmapMode::Contain,
                             active ? dashboard::paper : dashboard::ink);
      pixelui::text(screen.target(), fui::makeRect(r.x + 74, r.y, r.width - 86, r.height), label, 0,
                    active ? fui::Color::White : fui::Color::Black);
    }
  };
  auto r = group(0, tr(STR_UI_OPPONENT_974974));
  choice(r, 0, 2, tr(STR_UI_COMPUTER_9E47DA), SettingValue::Computer, model.opponent == Opponent::Computer,
         &compactart::ChessChip);
  choice(r, 1, 2, tr(STR_CHESS_PERSON), SettingValue::Person, model.opponent != Opponent::Computer,
         &compactart::ChessPerson);
  r = group(1, tr(STR_UI_LEVEL_F481CC));
  choice(r, 0, 3, tr(STR_UI_EASY_493F11), SettingValue::Easy, model.level == 0);
  choice(r, 1, 3, tr(STR_CHESS_NORMAL), SettingValue::Normal, model.level == 1);
  choice(r, 2, 3, tr(STR_UI_HARD_A06A58), SettingValue::Hard, model.level == 2);
  r = group(2, tr(STR_CHESS_YOUR_COLOUR));
  choice(r, 0, 2, tr(STR_UI_WHITE_40D23F), SettingValue::White, model.humanPlaysWhite, &compactart::ChessPawnWhite);
  choice(r, 1, 2, tr(STR_UI_BLACK_A9414A), SettingValue::Black, !model.humanPlaysWhite, &compactart::ChessPawnBlack);
  r = group(3, tr(STR_CHESS_HINTS_HEADING));
  for (int n = 0; n < 2; ++n) {
    const auto box = fui::makeRect(r.x, r.y + n * 76, r.width, 64);
    dashboard::frame(screen, box, 2);
    dashboard::hit(screen, box, ActionSettingValue, int(n ? SettingValue::Highlights : SettingValue::Hints));
    compactart::draw(screen, fui::makeRect(box.x + 12, box.y + 16, 32, 32), compactart::ChessHint);
    caps(screen, fui::makeRect(box.x + 62, box.y + 2, box.width - 118, 60),
         n ? tr(STR_CHESS_HINTS_CAPTION) : tr(STR_CHESS_SHOW_MOVES), 0, 2);
    dashboard::check(screen, fui::makeRect(box.right() - 42, box.y + 17, 30, 30),
                     n ? model.highlightSquares : model.showHints);
  }
  const auto close = fui::makeRect(body.x, body.bottom() - 48, wide ? width : body.width, 48);
  dashboard::button(screen, close, tr(STR_UI_BACK_TO_MENU_398BBB), ActionCloseSettings, 0, true);
  gamepanel::mark(screen, fui::makeRect(close.x + 14, close.y + 8, 32, 32), gamepanel::Mark::Back, true);
}

BoardLayout boardLayout(const fui::DeviceContext& device) {
  BoardLayout g;
  const auto b = pixelui::geometry(device).body;
  const bool wide = b.width > b.height;
  g.cell = std::max(
      1,
      std::min(wide ? int(b.width) * 9 / 20 : int(b.width) - 20, wide ? int(b.height) - 32 : int(b.height) - 248) / 8);
  const int side = g.cell * 8;
  g.board = fui::makeRect(wide ? b.x + 20 : b.x + 18, wide ? b.y + 8 : b.y + 50, side, side);
  const int x = wide ? g.board.right() + 30 : b.x, w = wide ? b.right() - x : b.width;
  const int panelTop = wide ? g.board.y + (side - 226) / 2 : b.y;
  g.status = fui::makeRect(x, panelTop, w, 42);
  const int lastY = wide ? panelTop + 60 : g.board.bottom() + 30;
  g.lastMove = fui::makeRect(x, lastY, w, 48);
  g.losses = fui::makeRect(x, lastY + 58, w, 108);
  return g;
}
fui::Rect buildBoardChrome(toybox::Screen& screen, const BoardModel& model) {
  frontdoor::begin(screen, tr(STR_UI_CHESS_902609));
  const auto g = boardLayout(screen.device());
  dashboard::frame(screen, g.status, 3);
  const int tail = std::min(108, int(g.status.width) / 3);
  caps(screen, fui::makeRect(g.status.x + 10, g.status.y, g.status.width - tail - 18, g.status.height), model.status, 0,
       1);
  caps(screen, fui::makeRect(g.status.right() - tail, g.status.y, tail, g.status.height), model.level, 0, 1,
       fui::TextAlign::Center);
  const int rule = g.status.right() - tail - 4;
  screen.target().line({int16_t(rule), int16_t(g.status.y + 8)}, {int16_t(rule), int16_t(g.status.bottom() - 8)}, 2,
                       dashboard::ink);
  for (int n = 0; n < 8; ++n) {
    char rank[2] = {char((model.whiteAtBottom ? '8' : '1') + (model.whiteAtBottom ? -n : n)), 0};
    char file[2] = {char((model.whiteAtBottom ? 'a' : 'h') + (model.whiteAtBottom ? n : -n)), 0};
    fittext::draw(screen.target(), fui::makeRect(g.board.x - 22, g.board.y + n * g.cell, 18, g.cell), rank, 3, 1,
                  fui::TextAlign::Center);
    fittext::draw(screen.target(), fui::makeRect(g.board.x + n * g.cell, g.board.bottom() + 3, g.cell, 24), file, 3, 1,
                  fui::TextAlign::Center);
  }
  dashboard::frame(screen, g.lastMove, 3);
  const int left = g.lastMove.width / 2;
  caps(screen, fui::makeRect(g.lastMove.x + 8, g.lastMove.y, left - 12, 48), tr(STR_CHESS_LAST_MOVE), 0, 1);
  fittext::draw(screen.target(), fui::makeRect(g.lastMove.x + left, g.lastMove.y, g.lastMove.width - left, 48),
                model.lastMove, 1, 1, fui::TextAlign::Center);
  screen.target().line({int16_t(g.lastMove.x + left), int16_t(g.lastMove.y + 8)},
                       {int16_t(g.lastMove.x + left), int16_t(g.lastMove.bottom() - 8)}, 2, dashboard::ink);
  dashboard::frame(screen, g.losses, 3);
  caps(screen, fui::makeRect(g.losses.x + 8, g.losses.y, g.losses.width - 16, 30), tr(STR_CHESS_LOST_PIECES), 0, 1);
  static constexpr int types[] = {chess::Pawn, chess::Knight, chess::Rook, chess::Bishop, chess::Queen};
  const int labelWidth = std::min(90, int(g.losses.width) / 4), stride = (g.losses.width - labelWidth) / 5;
  for (int c = 0; c < 2; ++c) {
    const int y = g.losses.y + 32 + c * 36;
    screen.target().line({g.losses.x, int16_t(y)}, {g.losses.right(), int16_t(y)}, 2, dashboard::ink);
    fittext::draw(screen.target(), fui::makeRect(g.losses.x + 6, y, labelWidth - 8, 36),
                  c ? tr(STR_UI_BLACK_A9414A) : tr(STR_UI_WHITE_40D23F), 3, 1);
    for (int n = 0; n < 5; ++n) {
      const int x = g.losses.x + labelWidth + n * stride;
      chessstyle::draw(screen.target(), fui::makeRect(x, y + 3, std::min(28, stride / 2), 30), types[n], c);
      char count[8];
      std::snprintf(count, sizeof(count), "×%u", model.losses ? unsigned(model.losses[c][types[n]]) : 0u);
      fittext::draw(screen.target(),
                    fui::makeRect(x + std::min(28, stride / 2), y, stride - std::min(28, stride / 2), 36), count, 3, 1);
    }
  }
  return g.board;
}
void buildResult(toybox::Screen& screen, const ResultModel& model) {
  frontdoor::begin(screen, tr(STR_UI_CHESS_902609));
  const bool won = !model.draw && model.winnerBlack == model.humanBlack;
  const auto art = model.draw ? fui::BitmapRef{}
                   : won      ? (model.winnerBlack ? resultart::ChessWinBlack : resultart::ChessWinWhite)
                              : (model.winnerBlack ? resultart::ChessLossWhite : resultart::ChessLossBlack);
  matchresult::draw(screen, screen.body(),
                    model.draw ? tr(STR_UI_A_DRAW_4D5967)
                    : won      ? tr(STR_MATCH_WON)
                               : tr(STR_MATCH_LOST),
                    model.draw          ? ""
                    : model.winnerBlack ? tr(STR_RESULT_BLACK_WINS)
                                        : tr(STR_RESULT_WHITE_WINS),
                    model.badge, won ? tr(STR_DRAUGHTS_WON) : tr(STR_DRAUGHTS_RETRY), art, ActionPlayAgain,
                    ActionResultReview, ActionResultMenu);
}

int startRows(const StartModel& model) { return model.hasSavedGame ? 4 : 3; }

StartRow startRowAt(const StartModel& model, const int visibleIndex) {
  const int count = startRows(model);
  const int clamped = visibleIndex < 0 ? 0 : (visibleIndex >= count ? count - 1 : visibleIndex);
  // With no game to continue, the first row is NEW GAME and everything shifts.
  return static_cast<StartRow>(model.hasSavedGame ? clamped : clamped + 1);
}

const char* startRowLabel(const StartRow row) {
  switch (row) {
    case StartRow::Continue:
      return tr(STR_UI_CONTINUE_FF3DBB);
    case StartRow::NewGame:
      return tr(STR_UI_NEW_GAME_F6F041);
    case StartRow::Puzzles:
      return tr(STR_CHESS_PUZZLES);
    case StartRow::Settings:
      return tr(STR_UI_SETTINGS_C4B924);
    default:
      return "";
  }
}

fui::Rect buildStartMenu(toybox::Screen& screen, const StartModel& model) {
  frontdoor::begin(screen, tr(STR_UI_CHESS_902609));
  const int count = startRows(model);
  const auto g = frontdoor::layout(screen.device(), count, false, false, true);

  frontdoor::text(screen, g.heading, model.hasSavedGame ? tr(STR_GAME_PAUSED) : tr(STR_UI_NOT_STARTED_9462D7), 1);

  frontdoor::Row rows[static_cast<int>(StartRow::Count)]{};
  for (int n = 0; n < count; ++n) {
    const auto row = startRowAt(model, n);
    rows[n].label = startRowLabel(row);
    rows[n].action = ActionStartRow;
    rows[n].value = n;
    rows[n].symbol = row == StartRow::NewGame ? frontdoor::Symbol::Restart : frontdoor::Symbol::Play;
    if (row == StartRow::Puzzles) rows[n].symbol = frontdoor::Symbol::Grid;
    if (row == StartRow::Settings) rows[n].symbol = frontdoor::Symbol::Settings;
    if (row == StartRow::Continue) rows[n].detail = model.continueDetail;
  }
  frontdoor::rows(screen, g, rows, count, model.selected);
  return g.preview;
}

}  // namespace chessui
