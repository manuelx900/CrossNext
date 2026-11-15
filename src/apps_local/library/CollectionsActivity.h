#pragma once
#include <Bitmap.h>
#include <LibraryIndexFile.h>

#include <array>
#include <atomic>

#include "../ui/ToyboxScreen.h"
#include "CollectionFolder.h"
#include "CollectionFolderStates.h"
#include "CollectionStore.h"
#include "LibraryControls.h"
#include "activities/UiListActivity.h"
#include "components/OptionPopup.h"

class CollectionsActivity final : public UiListActivity {
 public:
  CollectionsActivity(GfxRenderer& renderer, MappedInputManager& input, std::string book = {}, std::string folder = {});
  void onEnter() override;
  void onExit() override;
  void render(RenderLock&& lock) override;
  void runIdleWork() override;
  std::optional<uint32_t> nextLoopWakeDelayMs(uint32_t) const override {
    return editorScanPending_.load() ? std::optional<uint32_t>(20) : std::nullopt;
  }

 protected:
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int row) override;
  void onRowLongPress(int row) override;
  void onBackButton() override;
  bool handleCustomInput() override;
  void drawChrome() override {}
  void drawFooter() override {}

 private:
  enum class View : uint8_t { Catalog, Books, Editor, Destination };
  enum class Transfer : uint8_t { Add, Move };
  View view_ = View::Catalog;
  Transfer transfer_ = Transfer::Add;
  collections::Store store_;
  collections::Selection selection_;
  collections::Folder folder_;
  collections::FolderScan editorScan_;
  collections::FolderStates editorFolders_;
  library::LibraryIndexFile index_;
  HalMemory::PsramBuffer rows_;
  uint16_t catalog_[collections::MaxCollections + 2]{};
  unsigned catalogCount_ = 0, bookCount_ = 0;
  uint32_t collection_ = 0, origin_ = 0;
  bool grid_ = true, dirty_ = false;
  int editorFirst_ = -1, editorCount_ = 0, editorScanRow_ = -1;
  uint16_t editorFilesValid_ = 0, editorFilesChecked_ = 0, editorUnavailable_ = 0, editorKnownRows_ = 0;
  std::atomic<bool> editorInterrupted_{true}, editorScanPending_{false};
  bool editorNeedsPaint_ = false;
  unsigned long editorInputAt_ = 0, editorPaintAt_ = 0;
  uint64_t editorSource_ = 0;
  std::string editorScanPath_;
  struct EditorLocation {
    int selected = 0, top = 0;
    uint16_t length = 0;
  };
  std::array<EditorLocation, 32> editorHistory_{};
  unsigned editorDepth_ = 0;
  std::string incomingBook_, initialFolder_, directory_ = "/", path_, entry_, title_, collectionName_;
  libraryui::HeaderWork header_;
  freeink::ui::ListItem items_[10]{};
  std::string titles_[10], values_[10];
  OptionPopup popup_;
  HalFile coverFile_;
  Bitmap cover_{coverFile_};
  std::string pendingCover_;
  int coverHeight_ = 0, coverPage_ = -1;
  uint16_t attempted_ = 0;
  unsigned long coverPaint_ = 0;
  std::atomic<bool> pending_{false}, coverWork_{false};
  enum class BookPhase : uint8_t { Shell, Preparing, Ready };
  enum class CoverPresentation : uint8_t { Unannounced, Notice, Loading, Complete };
  std::atomic<BookPhase> bookPhase_{BookPhase::Ready};
  std::atomic<CoverPresentation> coverPresentation_{CoverPresentation::Unannounced};
  bool unresolvedCovers_ = false;
  void changed();
  void fail();
  void catalog();
  uint32_t catalogId(unsigned row) const;
  const char* nameFor(uint32_t id) const;
  void create();
  void rename();
  void edit();
  bool books(uint32_t id);
  bool loadBooks();
  void generatePendingCover();
  bool loadFinished();
  bool bookPath(unsigned row, std::string& path);
  void bookOptions(unsigned row);
  void collectionOptions(unsigned row);
  void save();
  void chooseDestination(const std::string& path, Transfer transfer);
  void transfer(uint32_t target);
  void buildCatalog(toybox::Screen& screen);
  void buildBooks(toybox::Screen& screen);
  void buildEditor(toybox::Screen& screen);
  void buildRows(toybox::Screen& screen);
  void changeDirectory(const std::string& path);
  void toggleEntry(unsigned row);
  void inspectEditorFolders();
  void cancelEditorScan();
  void settleEditorFolder();
  static bool continueEditorScan(void* context);
  void drawCover(toybox::Screen& screen, freeink::ui::Rect box, const std::string& path, int slot, bool defer);
};
