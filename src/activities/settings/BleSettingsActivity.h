#pragma once
#include <BlePageTurner.h>

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

class BleSettingsActivity final : public UiListActivity {
 public:
  explicit BleSettingsActivity(GfxRenderer& renderer, MappedInputManager& input)
      : UiListActivity("BleSettings", renderer, input, false, true) {}
  void onEnter() override;
  void loop() override;
  bool allowsRemoteControl() const override { return false; }
  bool preservesRemoteConnection() const override { return true; }

 private:
  enum class View : uint8_t { Menu, Scan, Paired, Bindings, Capture, Actions, Apps, Help };
  View view = View::Menu;
  int listCount() const override;
  const char* headerTitle() const override;
  void buildScreen(UiScreen&) override;
  void activateIndex(int) override;
  void onBackButton() override;
  void showView(View);
  void refresh();
  void provideRow(uint16_t, freeink::ui::ListItem&);
  uint32_t signature() const;
  void startCapture();
  void stepCapture();
  bool saveBinding(bleturner::Action, const char* title = nullptr);
  void restoreButton();
  void confirmRemoval(int index);
  void removeBinding(const char* address, uint32_t code, bool held);
  void buildBindings(UiScreen&, freeink::ui::ListProps&);
  const char* actionLabel(const bleturner::RemoteTable&, bleturner::Action) const;
  std::vector<freeink::ui::ListItem> rows;
  std::vector<std::string> labels;
  char status[80]{}, pairingCode[96]{}, learnAddr[18]{};
  char buttonLabels[bleturner::kMaxBindings][16]{};
  const char* note = nullptr;
  uint32_t shownSignature = 0, radioVisit = 0;
  unsigned long lastPollMs = 0, learnSinceMs = 0;
  uint32_t learnCode = 0, learnPressMs = 0;
  bool hold = false, pairingCodeVisible = false;
};
