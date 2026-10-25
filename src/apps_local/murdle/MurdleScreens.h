#pragma once
#include "../ui/ToyboxScreen.h"
#include "MurdleCampaign.h"
#include "MurdleCore.h"
namespace murdleui {
namespace fui = freeink::ui;
enum : fui::ActionId {
  ActionPlay = 1,
  ActionNewCase = 2,
  ActionSettings = 3,
  ActionHowTo = 4,
  ActionBack = 5,
  ActionFace = 6,
  ActionPage = 7,
  ActionClue = 8,
  ActionAccuse = 9,
  ActionPick = 10,
  ActionConfirm = 11,
  ActionKeepLooking = 12,
  ActionDone = 13,
  ActionTier = 14,
  ActionConfirmNew = 15,
  ActionCancel = 16,
  ActionGrid = 17,
  ActionCampaign = 18,
  ActionCampaignPage = 19,
  ActionOpenCampaign = 20,
  ActionRetry = 21,
  ActionUndo = 22,
  ActionInfoCategory = 23,
  ActionRetryCase = 24,
  ActionSolution = 25,
  ActionReviewAccusation = 26,
  ActionResetProgress = 27,
  ActionConfirmReset = 28
};
enum class Face : uint8_t { Clues, Grid, Info };
constexpr int kFaceCount = 3;
struct GridLayout {
  bool valid = false;
  int groups = 0, items = 0;
  int16_t cell = 0, cellWidth = 0, originX = 0, originY = 0, gutter = 0, headerH = 0;
  uint8_t colCat[murdle::kMaxCats]{}, rowCat[murdle::kMaxCats]{};
  bool blockLive(int r, int c) const { return r + c < groups; }
  int16_t cellX(int col) const { return originX + col * (cellWidth ? cellWidth : cell); }
  int16_t cellY(int row) const { return originY + row * cell; }
};
struct GridCell {
  int catA = 0, itemA = 0, catB = 0, itemB = 0;
};
bool cellAt(const GridLayout&, int, int, GridCell&);
struct CaseModel {
  const murdle::Puzzle* puzzle = nullptr;
  const murdle::Marks* marks = nullptr;
  Face face = Face::Clues;
  int page = 0;
  uint32_t struck = 0;
  int caseNumber = 1;
  bool solved = false;
  const char* notice = nullptr;
  int wrongAccusations = 0;
  bool canUndo = false, solution = false;
  int lastA = -1, lastB = 0, lastIA = 0, lastIB = 0;
};
struct CaseReport {
  GridLayout grid;
  int pages = 1, page = 0;
};
CaseReport buildCase(toybox::Screen&, const CaseModel&);
struct ClueLayout {
  int16_t top[murdle::kMaxClues + 1]{};
  uint8_t index[murdle::kMaxClues]{};
  int count = 0;
};
ClueLayout lastClueLayout();
struct MenuModel {
  bool hasCase = false, caseSolved = false;
  murdle::Tier tier = murdle::Tier::Elementary;
  int caseNumber = 0, solvedCount = 0, wrongCount = 0;
  const murdle::Puzzle* puzzle = nullptr;
  const murdle::Marks* marks = nullptr;
  int cluesTicked = 0;
  uint32_t record = 0;
  int wrongAccusations = 0;
};
void buildMenu(toybox::Screen&, const MenuModel&);
void buildCampaign(toybox::Screen&, const murdlecampaign::Progress&, int page, int selected, int active = -1,
                   murdle::Tier filter = murdle::Tier::Elementary);
struct SettingsModel {
  murdle::Tier tier = murdle::Tier::Elementary;
  bool caseOpen = false;
};
void buildSettings(toybox::Screen&, const SettingsModel&);
struct AccuseModel {
  const murdle::Puzzle* puzzle = nullptr;
  static constexpr uint8_t kNothingPicked = 255;
  uint8_t picks[murdle::kMaxCats] = {255, 255, 255, 255};
  int wrongAccusations = 0;
  bool complete() const;
};
void buildAccuse(toybox::Screen&, const AccuseModel&);
struct VerdictModel {
  const murdle::Puzzle* puzzle = nullptr;
  bool right = false, nextCase = false;
  int wrongAccusations = 0;
  uint8_t picks[murdle::kMaxCats]{};
};
void buildVerdict(toybox::Screen&, const VerdictModel&);
void buildConfirmNew(toybox::Screen&);
void buildConfirmReset(toybox::Screen&, int completed);
struct HowToModel {
  int page = 0;
};
int howToPages(bool wide = false);
void buildHowTo(toybox::Screen&, const HowToModel&);
void buildStorageError(toybox::Screen&, bool);
const char* tierName(murdle::Tier);
const char* tierShape(murdle::Tier);
}  // namespace murdleui
