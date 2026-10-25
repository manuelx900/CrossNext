#include <algorithm>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>

#include "MurdleCampaign.h"

namespace ui {
enum { ActionOpenCampaign = 1, ActionCampaignPage, ActionTier };
}
struct MappedInputManager {
  enum class Button { Up, Down, Confirm, Left, Right };
  Button released = Button::Down;
  bool wasReleased(Button key) const { return key == released; }
};
struct CampaignButtons {
  enum class View { Cases, Case };
  View view = View::Cases;
  MappedInputManager mappedInput;
  murdlecampaign::Progress campaign;
  murdle::Tier campaignTier = murdle::Tier::Elementary;
  int campaignSelected = 0, campaignPage = 0, opened = -1, paints = 0;
  void requestUpdate() { ++paints; }
  void routeAction(int action, int value) {
    switch (action) {
      case ui::ActionOpenCampaign:
        opened = value;
        break;
#include "campaign_filter.generated.inc"
#include "campaign_page.generated.inc"
    }
  }
  void step() {
#include "campaign_buttons.generated.inc"
  }
  void enter() {
#include "campaign_entry.generated.inc"
  }
};

int main() {
  CampaignButtons app;
  int checks = 0;
  assert(app.campaign.next() == 0);
  app.campaignSelected = 70;
  app.enter();
  assert(app.campaignSelected == 0 && app.campaignPage == 0 && app.campaignTier == murdle::Tier::Elementary);
  ++checks;
  for (int tier = 0; tier < murdle::kTierCount; ++tier) {
    app.routeAction(ui::ActionTier, tier);
    const int first = tier * murdlecampaign::kCasesPerTier;
    assert(app.campaignSelected == first && app.campaignPage == 0);
    app.mappedInput.released = MappedInputManager::Button::Down;
    for (int n = 1; n < 110; ++n) {
      app.step();
      assert(app.campaignSelected == first + std::min(99, n));
      assert(app.campaignPage == (app.campaignSelected - first) / 16);
      checks += 2;
    }
    app.mappedInput.released = MappedInputManager::Button::Confirm;
    app.step();
    assert(app.opened == first + 99);
    ++checks;
    app.mappedInput.released = MappedInputManager::Button::Up;
    for (int n = 1; n < 110; ++n) {
      app.step();
      assert(app.campaignSelected == first + std::max(0, 99 - n));
      assert(app.campaignPage == (app.campaignSelected - first) / 16);
      checks += 2;
    }
    for (int pg = 0; pg < 9; ++pg) {
      app.routeAction(ui::ActionCampaignPage, 1);
      assert(app.campaignPage == std::min(6, pg + 1));
      assert(app.campaignSelected == first + app.campaignPage * 16);
      checks += 2;
    }
    app.campaignSelected = first + 16;
    app.step();
    assert(app.campaignSelected == first + 15 && app.campaignPage == 0);
    checks += 2;
    assert(app.campaign.next() == 0);
  }
  const int selected = app.campaignSelected;
  app.routeAction(ui::ActionTier, -1);
  app.routeAction(ui::ActionTier, 4);
  assert(app.campaignSelected == selected);
  app.view = CampaignButtons::View::Case;
  app.routeAction(ui::ActionTier, 0);
  assert(app.campaignSelected == selected);
  app.view = CampaignButtons::View::Cases;
  for (int n = 0; n < murdlecampaign::kCaseCount; ++n) {
    app.campaign.finish(n);
    app.enter();
    assert(app.campaignSelected == std::min(399, n + 1));
    assert(app.campaignPage == (app.campaignSelected % 100) / 16);
    assert(int(app.campaignTier) == app.campaignSelected / 100);
    checks += 3;
  }
  std::printf("Murdle production campaign buttons: %d checks, 0 failed\n", checks);
}
