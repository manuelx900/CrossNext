#pragma once
#include "../ui/ToyboxScreen.h"
#include "QuestAssets.h"
#include "QuestCore.h"
namespace questui {
enum : freeink::ui::ActionId {
  Tab = 1,
  Claim,
  Outfit,
  Read,
  Gender,
  Category,
  Page,
  Milestone,
  Preview,
  Back,
  InventoryView,
  LevelContinue
};
struct Model {
  const quest::Profile* profile = nullptr;
  const readingstats::Summary* summary = nullptr;
  int tab = 0, today = 0;
  bool loading = false, failed = false, denied = false;
  int category = 0, page = 0, preview = -1, questPage = 0;
  bool ownedGear = false;
  sdart::Pack* assets = nullptr;
};
struct Work {
  freeink::ui::ButtonProps button;
  freeink::ui::TabItem tabs[quest::kSlotCount]{};
  int page = 0, pages = 1;
  char label[160]{}, name[96]{};
  char tabLabels[quest::kSlotCount][32]{};
};
const char* rankTitle(uint32_t level);
void buildLevelUp(toybox::Screen&, const quest::Profile&, bool saveFailed, Work&);
void build(toybox::Screen&, const Model&, Work&);
}  // namespace questui
