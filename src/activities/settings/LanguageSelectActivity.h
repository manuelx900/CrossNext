#pragma once

#include <LanguagePack.h>

#include "activities/UiListActivity.h"

class LanguageSelectActivity final : public UiListActivity {
 public:
  explicit LanguageSelectActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  void onEnter() override;
  void onExit() override;

 private:
  static constexpr int BUILTIN_COUNT = 2;
  languagepack::Catalog catalog;
  bool catalogFailed = false;
  int listCount() const override { return BUILTIN_COUNT + static_cast<int>(catalog.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void applySelection(int index);
  bool isActive(int index) const;
  const char* headerTitle() const override;
  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);
};
