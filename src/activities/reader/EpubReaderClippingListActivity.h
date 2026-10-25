#pragma once

#include <Epub.h>

#include <array>
#include <atomic>

#include "activities/UiListActivity.h"
#include "clippings/ClippingPreview.h"
#include "components/OptionPopup.h"

class EpubReaderClippingListActivity final : public UiListActivity {
 public:
  EpubReaderClippingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                 const std::shared_ptr<Epub>& book = {});

  void onEnter() override;
  void render(RenderLock&&) override;

 private:
  std::shared_ptr<Epub> book;
  freeink::ui::ButtonProps savedButton;
  std::atomic<int> savedPageRows{1};
  void navigateButtons() override;
  std::array<std::array<char, 96>, 8> positions{};
  void drawChrome() override;
  static void onSavedAction(const freeink::ui::ActionEvent& event, void* user);
  void rebuildRows();
  void refreshRowWindow(int start);
  void openSelected();
  void readSelected();
  void showDeleteConfirmation();
  void deleteSelected();

  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onRowLongPress(int index) override;
  bool handleCustomInput() override;
  bool handleButtons() override;
  const char* headerTitle() const override;

  static constexpr int ROW_WINDOW = 8;
  std::array<std::array<char, clippingPreview::BUFFER_BYTES>, ROW_WINDOW> previews{};
  std::array<size_t, ROW_WINDOW> previewLengths{};
  std::array<freeink::ui::ListItem, ROW_WINDOW> rowItems;
  int windowStart = -1;
  int windowCount = 0;
  OptionPopup confirmPopup;
  bool confirmingDelete = false;
  bool initialListRender = true;
};
