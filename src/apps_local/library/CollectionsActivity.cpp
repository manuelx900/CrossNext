#include "CollectionsActivity.h"

#include <ContentVisibility.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <LibraryBuilder.h>
#include <Logging.h>
#include <Memory.h>
#include <Utf8.h>
#include <Xtc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>

#include "CollectionActions.h"
#include "CollectionScreens.h"
#include "LibraryEntryBackdrop.h"
#include "LibraryFinishedBooks.h"
#include "LibraryLoadingNotice.h"
#include "RecentBooksStore.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "apps_local/home/CoverSceneCache.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/UITheme.h"
#include "components/icons/listIcons.h"
namespace fui = freeink::ui;
using namespace collectionui;
namespace {
constexpr unsigned long EditorIdleMs = 80, EditorPaintBatchMs = 180;
uint64_t editorSourceRevision() {
  return uint64_t(library::libraryContentRevision()) << 32 | homecover::sceneRevision();
}
bool knownEditorFolder(void* context, const char* path, collections::FolderSummary& summary) {
  return static_cast<collections::FolderStates*>(context)->get(path, summary, false);
}
void basename(const std::string& path, std::string& name) {
  const auto slash = path.rfind('/');
  name.assign(path, slash == std::string::npos ? 0 : slash + 1, std::string::npos);
  const auto dot = name.rfind('.');
  if (dot != std::string::npos) name.resize(dot);
}
}  // namespace
CollectionsActivity::CollectionsActivity(GfxRenderer& renderer, MappedInputManager& input, std::string book,
                                         std::string folder)
    : UiListActivity("Collections", renderer, input, true),
      incomingBook_(std::move(book)),
      initialFolder_(std::move(folder)) {
  setInterfaceFontSection(menufont::Section::Library);
}
void CollectionsActivity::onEnter() {
  static_assert(Tab == ACTION_USER);
  RenderLock lock(*this);
  UiListActivity::onEnter();
  grid_ = SETTINGS.libraryCollectionGrid != 0;
  path_.reserve(collections::PathBytes);
  entry_.reserve(collections::PathBytes);
  title_.reserve(collections::PathBytes);
  directory_.reserve(collections::PathBytes);
  pendingCover_.reserve(collections::PathBytes);
  editorScanPath_.reserve(collections::PathBytes);
  for (auto& title : titles_) title.reserve(512);
  for (auto& value : values_) value.reserve(64);
  const auto action = [](const fui::ActionEvent& event, void* ctx) {
    auto& self = *static_cast<CollectionsActivity*>(ctx);
    RenderLock lock(self);
    self.app.clearTapFlash();
    switch (static_cast<Action>(event.action)) {
      case Tab:
        if (event.value != 1) {
          self.setResult(ActivityResult(MenuResult{event.value}));
          self.finish();
        }
        break;
      case Command:
        switch (static_cast<CommandValue>(event.value)) {
          case ToggleView: {
            const auto previous = SETTINGS.libraryCollectionGrid;
            SETTINGS.libraryCollectionGrid = !self.grid_;
            if (!SETTINGS.saveToFile()) {
              SETTINGS.libraryCollectionGrid = previous;
              self.fail();
              break;
            }
            self.grid_ = SETTINGS.libraryCollectionGrid != 0;
            self.nav.reset();
            self.changed();
            break;
          }
          case New:
            self.create();
            break;
          case Menu:
            if (self.view_ == View::Books) self.collectionOptions(0);
            break;
          case Save:
            self.save();
            break;
        }
        break;
      case Options:
        if (self.view_ == View::Catalog)
          self.collectionOptions(event.value);
        else if (self.view_ == View::Books)
          self.collectionOptions(0);
        else if (self.view_ == View::Editor)
          self.toggleEntry(event.value);
        break;
      case Page:
        self.nav.requestSelection(event.value);
        self.changed();
        break;
    }
    self.requestUpdate();
  };
  bindActions(app, action, this);
  if (!store_.open()) {
    fail();
    return;
  }
  catalog();
  if (!incomingBook_.empty()) view_ = View::Destination;
  if (!initialFolder_.empty()) create();
}
void CollectionsActivity::onExit() {
  cancelEditorScan();
  editorFolders_.close();
  folder_.close();
  selection_.close();
  index_.close();
  coverFile_.close();
  rows_.reset();
  Activity::onExit();
}
void CollectionsActivity::changed() {
  const auto section = view_ == View::Editor ? menufont::Section::Files : menufont::Section::Library;
  if (interfaceFontContext.section != section) setInterfaceFontSection(section);
  if (view_ != View::Editor) {
    cancelEditorScan();
    editorFolders_.close();
  }
  resetUi();
  pending_ = false;
  coverWork_ = false;
  pendingCover_.clear();
  coverPage_ = -1;
  attempted_ = 0;
  coverPresentation_ = CoverPresentation::Unannounced;
  requestUpdate();
}
void CollectionsActivity::fail() {
  LOG_ERR("COLL", "Collection operation failed");
  const char* options[] = {tr(STR_CONFIRM)};
  popup_.showMessage(tr(STR_COLLECTIONS), tr(STR_COLLECTION_FAILED), options, 1, 0, [](int) {});
  requestUpdate();
}
const char* CollectionsActivity::nameFor(uint32_t id) const {
  if (id == collections::Favorites) return tr(STR_COLLECTION_FAVORITES);
  if (id == collections::Finished) return tr(STR_COLLECTION_FINISHED);
  const auto* collection = store_.find(id);
  return collection ? collection->name : "";
}
uint32_t CollectionsActivity::catalogId(unsigned row) const {
  return row < catalogCount_ ? store_.at(catalog_[row]).id : 0;
}
void CollectionsActivity::catalog() {
  bookPhase_ = BookPhase::Ready;
  cancelEditorScan();
  editorFolders_.close();
  editorDepth_ = 0;
  selection_.close();
  folder_.close();
  rows_.reset();
  index_.close();
  bookCount_ = 0;
  catalogCount_ = store_.order(catalog_, false);
  view_ = incomingBook_.empty() ? View::Catalog : View::Destination;
  collection_ = 0;
  nav.reset();
  changed();
}
int CollectionsActivity::listCount() const {
  switch (view_) {
    case View::Catalog:
    case View::Destination:
      return catalogCount_;
    case View::Books:
      return bookCount_;
    case View::Editor:
      return folder_.count();
  }
  return 0;
}
void CollectionsActivity::create() {
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_COLLECTION_NEW), "", 39, InputType::Text);
  if (!keyboard) {
    fail();
    return;
  }
  keyboard->setCenteredText(true);
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    RenderLock lock(*this);
    if (result.isCancelled) return;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (!text) return;
    const auto name = utf8ComposeNfc(text->text);
    const auto id = store_.create(name.c_str());
    if (!id) {
      fail();
      return;
    }
    if (!incomingBook_.empty()) {
      transfer(id);
      return;
    }
    collection_ = id;
    collectionName_ = name;
    edit();
    if (!initialFolder_.empty() && view_ == View::Editor) {
      collections::FolderSummary summary;
      if (!collections::selectFolder(selection_, initialFolder_, true, &summary))
        fail();
      else
        editorFolders_.put(initialFolder_.c_str(), summary);
      dirty_ = true;
      changeDirectory(initialFolder_);
      initialFolder_.clear();
    }
  });
}
void CollectionsActivity::rename() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_RENAME), collectionName_, 39,
                                                           InputType::Text);
  if (!keyboard) {
    fail();
    return;
  }
  keyboard->setCenteredText(true);
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    RenderLock lock(*this);
    if (result.isCancelled) return;
    const auto* text = std::get_if<KeyboardResult>(&result.data);
    if (text && store_.rename(collection_, utf8ComposeNfc(text->text).c_str())) {
      collectionName_ = nameFor(collection_);
      changed();
    } else
      fail();
  });
}
bool CollectionsActivity::books(uint32_t id) {
  collection_ = id;
  collectionName_ = nameFor(id);
  view_ = View::Books;
  const auto* collection = store_.find(id);
  bookCount_ = collection ? collection->count : 0;
  nav.reset();
  bookPhase_ = BookPhase::Shell;
  cancelEditorScan();
  if (!grid_) return loadBooks();
  changed();
  return true;
}
bool CollectionsActivity::loadBooks() {
  const auto id = collection_;
  cancelEditorScan();
  editorFolders_.close();
  editorDepth_ = 0;
  selection_.close();
  folder_.close();
  rows_.reset();
  index_.close();
  rows_ = HalMemory::allocatePsram(collections::MaxBooks * sizeof(uint32_t));
  if (!rows_) {
    LOG_ERR("COLL", "OOM: collection rows");
    fail();
    return false;
  }
  bookCount_ = 0;
  if (id == collections::Finished) {
    if (!loadFinished()) return false;
  } else if (!selection_.open(id, false)) {
    fail();
    return false;
  }
  for (unsigned n = 0; n < selection_.size(); ++n)
    if (selection_.selected(n)) reinterpret_cast<uint32_t*>(rows_.get())[bookCount_++] = n;
  store_.updateCount(id, bookCount_);
  collection_ = id;
  collectionName_ = nameFor(id);
  view_ = View::Books;
  bookPhase_ = BookPhase::Ready;
  nav.reset();
  changed();
  return true;
}
bool CollectionsActivity::loadFinished() {
  libraryui::FinishedCacheStamp stamp;
  uint64_t recentPaths = 0;
  for (const auto& book : RECENT_BOOKS.getBooks())
    recentPaths ^= library::clixPathHash(book.path.c_str(), book.path.size());
  const bool stamped = stamp.capture(library::libraryIndexPath(), recentPaths);
  if (stamped && selection_.openFinishedCache(false) && selection_.hasSnapshot() &&
      stamp.matches(selection_.generation())) {
    bool present = true;
    for (unsigned n = 0; n < selection_.size() && present; ++n) {
      present = selection_.path(n, path_) && Storage.exists(path_.c_str());
      if (!(n & 63)) delay(1);
    }
    if (present) return true;
  }
  selection_.close();
  auto finished = makeUniqueNoThrow<libraryui::FinishedBooks>();
  if (!finished || !finished->begin() || !selection_.openFinishedCache(true)) {
    fail();
    return false;
  }
  for (unsigned n = 0; n < selection_.size(); ++n) {
    if (!selection_.path(n, path_) || !selection_.set(path_.c_str(), false)) {
      fail();
      return false;
    }
  }
  if (!finished->empty()) {
    const auto include = [&](const std::string& path) {
      bool complete = false;
      if (!finished->contains(path, complete)) return !Storage.exists(path.c_str());
      return !complete || selection_.set(path.c_str(), true);
    };
    for (const auto& book : RECENT_BOOKS.getBooks()) {
      if (!include(book.path)) {
        fail();
        return false;
      }
    }
    if (index_.open(library::libraryIndexPath())) {
      library::ClixRecord record{};
      for (unsigned n = 0; n < index_.bookCount(); ++n) {
        if (!index_.readRecord(n, record) || !index_.readPath(record, path_) || !include(path_)) {
          fail();
          return false;
        }
        if (!(n & 31)) delay(1);
      }
      index_.close();
    }
  }
  if (!selection_.save()) {
    fail();
    return false;
  }
  if (stamped && !stamp.save(selection_.generation())) LOG_ERR("COLL", "Could not cache finished collection revision");
  return true;
}
bool CollectionsActivity::bookPath(unsigned row, std::string& path) {
  if (row >= bookCount_ || !rows_) return false;
  return selection_.path(reinterpret_cast<uint32_t*>(rows_.get())[row], path);
}
void CollectionsActivity::edit() {
  if (!editorFolders_.begin()) {
    fail();
    return;
  }
  editorFolders_.clear();
  editorSource_ = editorSourceRevision();
  editorDepth_ = 0;
  selection_.close();
  index_.close();
  rows_.reset();
  bookCount_ = 0;
  if (!selection_.open(collection_, true)) {
    editorFolders_.close();
    fail();
    return;
  }
  view_ = View::Editor;
  dirty_ = false;
  changeDirectory("/");
}
void CollectionsActivity::changeDirectory(const std::string& path) {
  cancelEditorScan();
  const std::string next = path;
  directory_ = next;
  if (!folder_.open(next, editorSource_)) {
    folder_.close();
    nav.reset();
    fail();
    changed();
    return;
  }
  nav.reset();
  editorFirst_ = -1;
  editorFilesValid_ = editorFilesChecked_ = 0;
  if (!folder_.hasSubfolders() && !selection_.mayContainSelected(directory_.c_str()))
    editorFolders_.put(directory_.c_str(), {collections::FolderSelection::None, folder_.count(), 0, true});
  changed();
}
void CollectionsActivity::cancelEditorScan() {
  editorInterrupted_ = true;
  editorScan_.close();
  editorScanRow_ = -1;
  editorScanPath_.clear();
  editorScanPending_ = false;
  editorNeedsPaint_ = false;
}
void CollectionsActivity::settleEditorFolder() {
  cancelEditorScan();
  collections::FolderSummary summary;
  if (editorFolders_.get(directory_.c_str(), summary)) return;
  if (!selection_.mayContainSelected(directory_.c_str())) {
    editorFolders_.put(directory_.c_str(), {});
    return;
  }
  const unsigned valid = editorFilesValid_ & ((1u << editorCount_) - 1);
  const unsigned checked = editorFilesChecked_ & valid;
  if (checked && checked != valid) {
    editorFolders_.put(directory_.c_str(), {collections::FolderSelection::Partial, unsigned(__builtin_popcount(valid)),
                                            unsigned(__builtin_popcount(checked)), false});
    return;
  }
  if (!folder_.hasSubfolders() && editorFirst_ == 0 && folder_.count() <= unsigned(editorCount_)) {
    const unsigned mask = (1u << folder_.count()) - 1;
    if ((editorFilesValid_ & mask) == mask) {
      const auto selected = unsigned(__builtin_popcount(editorFilesChecked_ & mask));
      summary = {selected ? selected == folder_.count() ? collections::FolderSelection::All
                                                        : collections::FolderSelection::Partial
                          : collections::FolderSelection::None,
                 folder_.count(), selected, true};
      editorFolders_.put(directory_.c_str(), summary);
      return;
    }
  }
  // Unknown state is resolved by the parent viewport idle work, never by Back.
}

void CollectionsActivity::toggleEntry(unsigned row) {
  if (view_ != View::Editor) return;
  cancelEditorScan();
  bool directory = false, checked = false;
  if (!folder_.entry(row, entry_, directory)) {
    fail();
    return;
  }
  path_ = directory_;
  if (path_.back() != '/') path_ += '/';
  path_ += entry_;
  const auto before = selection_.count();
  if (directory) {
    collections::FolderSummary previous;
    if (!editorFolders_.get(path_.c_str(), previous) && selection_.mayContainSelected(path_.c_str())) {
      if (!editorScan_.start(path_, collections::FolderScanMode::Inspect, &knownEditorFolder, &editorFolders_)) {
        fail();
        return;
      }
      for (;;) {
        auto state = collections::FolderSelection::None;
        const auto result = editorScan_.step(selection_, state);
        if (result == collections::FolderScanResult::Complete) {
          previous = editorScan_.summary();
          break;
        }
        if (result == collections::FolderScanResult::Error) {
          fail();
          return;
        }
        delay(1);
      }
    }
    const bool all = previous.state == collections::FolderSelection::All;
    collections::FolderSummary summary;
    dirty_ = true;
    const bool success = collections::selectFolder(selection_, path_, !all, &summary);
    const auto delta = int64_t(selection_.count()) - before;
    if (!success) {
      editorFolders_.invalidate(path_.c_str(), true);
      if (!selection_.count()) editorFolders_.changed(path_.c_str(), 0, false, false, true);
      fail();
      requestUpdate();
      return;
    }
    if (all && previous.counted) summary = {collections::FolderSelection::None, previous.total, 0, true};
    editorFolders_.changed(path_.c_str(), delta, true, !all, !selection_.count());
    editorFolders_.put(path_.c_str(), summary);
  } else {
    if (!selection_.has(path_.c_str(), checked) || !selection_.set(path_.c_str(), !checked)) {
      fail();
      return;
    }
    editorFolders_.changed(path_.c_str(), checked ? -1 : 1, false, !checked, !selection_.count());
    const int slot = int(row) - editorFirst_;
    if (slot >= 0 && slot < 10) {
      editorFilesValid_ |= 1u << slot;
      if (checked)
        editorFilesChecked_ &= ~(1u << slot);
      else
        editorFilesChecked_ |= 1u << slot;
    }
  }
  dirty_ = true;
  requestUpdate();
}
void CollectionsActivity::save() {
  if (view_ != View::Editor) return;
  if (!selection_.save()) {
    fail();
    return;
  }
  const auto count = selection_.count();
  if (!store_.updateCount(collection_, count)) {
    fail();
    return;
  }
  dirty_ = false;
  folder_.close();
  selection_.close();
  books(collection_);
}
void CollectionsActivity::activateIndex(int row) {
  RenderLock lock(*this);
  if (view_ == View::Books &&
      (bookPhase_ != BookPhase::Ready || (grid_ && coverPresentation_ != CoverPresentation::Complete)))
    return;
  app.clearTapFlash();
  if (row < 0 || row >= listCount()) return;
  switch (view_) {
    case View::Catalog:
      books(catalogId(row));
      break;
    case View::Destination: {
      const auto id = catalogId(row);
      if (id != collections::Finished) transfer(id);
      break;
    }
    case View::Books:
      if (bookPath(row, path_) && Storage.exists(path_.c_str())) {
        const auto book = path_;
        lock.unlock();
        onSelectBook(book);
      } else
        fail();
      break;
    case View::Editor: {
      bool directory = false;
      if (!folder_.entry(row, entry_, directory)) {
        fail();
        break;
      }
      path_ = directory_;
      if (path_.back() != '/') path_ += '/';
      path_ += entry_;
      if (directory) {
        if (editorDepth_ == editorHistory_.size()) {
          std::move(editorHistory_.begin() + 1, editorHistory_.end(), editorHistory_.begin());
          --editorDepth_;
        }
        editorHistory_[editorDepth_++] = {nav.selected.load(), nav.top, uint16_t(directory_.size())};
        changeDirectory(path_);
      } else
        toggleEntry(row);
      break;
    }
  }
}
void CollectionsActivity::onRowLongPress(int row) {
  RenderLock lock(*this);
  if (view_ == View::Books &&
      (bookPhase_ != BookPhase::Ready || (grid_ && coverPresentation_ != CoverPresentation::Complete)))
    return;
  app.clearTapFlash();
  if (view_ == View::Catalog)
    collectionOptions(row);
  else if (view_ == View::Books)
    bookOptions(row);
}
void CollectionsActivity::collectionOptions(unsigned row) {
  const auto id = view_ == View::Books ? collection_ : catalogId(row);
  const auto* value = store_.find(id);
  const bool favorite = value && value->favorite;
  const char* labels[] = {tr(STR_COLLECTION_EDIT), tr(STR_RENAME),
                          favorite ? tr(STR_COLLECTION_UNFAVORITE) : tr(STR_COLLECTION_FAVORITE), tr(STR_DELETE)};
  const char* smart[] = {tr(STR_COLLECTION_EDIT),
                         favorite ? tr(STR_COLLECTION_UNFAVORITE) : tr(STR_COLLECTION_FAVORITE)};
  if (id == collections::Finished) {
    const char* action[] = {favorite ? tr(STR_COLLECTION_UNFAVORITE) : tr(STR_COLLECTION_FAVORITE)};
    popup_.show(tr(STR_COLLECTIONS), action, 1, -1, [this, id, favorite](int choice) {
      if (choice == 0) {
        RenderLock lock(*this);
        if (!store_.favorite(id, !favorite))
          fail();
        else if (view_ == View::Catalog)
          catalog();
        requestUpdate();
      }
    });
    requestUpdate();
    return;
  }
  popup_.show(tr(STR_COLLECTIONS), nameFor(id), id == collections::Favorites ? smart : labels,
              id == collections::Favorites ? 2 : 4, -1, [this, id, favorite](int option) {
                RenderLock lock(*this);
                collection_ = id;
                collectionName_ = nameFor(id);
                if (id == collections::Favorites && option == 1) option = 2;
                switch (option) {
                  case 0:
                    edit();
                    break;
                  case 1:
                    rename();
                    break;
                  case 2:
                    if (!store_.favorite(id, !favorite))
                      fail();
                    else if (view_ == View::Catalog)
                      catalog();
                    break;
                  case 3: {
                    const char* confirm[] = {tr(STR_CANCEL), tr(STR_DELETE)};
                    popup_.showMessage(tr(STR_DELETE), tr(STR_COLLECTION_DELETE_NOTE), confirm, 2, -1,
                                       [this, id](int choice) {
                                         if (choice == 1) {
                                           RenderLock lock(*this);
                                           if (store_.erase(id))
                                             catalog();
                                           else
                                             fail();
                                         }
                                       });
                    break;
                  }
                }
                requestUpdate();
              });
  requestUpdate();
}
void CollectionsActivity::chooseDestination(const std::string& path, Transfer transfer) {
  incomingBook_ = path;
  transfer_ = transfer;
  origin_ = collection_;
  catalog();
  view_ = View::Destination;
}
void CollectionsActivity::transfer(uint32_t target) {
  if (target == collections::Finished || (transfer_ == Transfer::Move && target == origin_)) return;
  auto edit = makeUniqueNoThrow<collections::Selection>();
  if (!edit || !edit->open(target, true) || !edit->set(incomingBook_.c_str(), true) || !edit->save() ||
      !store_.updateCount(target, edit->count())) {
    fail();
    return;
  }
  edit->close();
  if (transfer_ == Transfer::Move && origin_ != collections::Finished) {
    if (!edit->open(origin_, true) || !edit->set(incomingBook_.c_str(), false) || !edit->save() ||
        !store_.updateCount(origin_, edit->count())) {
      fail();
      return;
    }
  }
  if (!origin_) {
    finish();
    return;
  }
  incomingBook_.clear();
  edit->close();
  books(origin_);
}
void CollectionsActivity::bookOptions(unsigned row) {
  if (!bookPath(row, path_)) return;
  const auto path = path_;
  bool favorite = false;
  if (!collections::isFavorite(path.c_str(), favorite)) {
    fail();
    return;
  }
  const char* labels[] = {tr(STR_OPEN), favorite ? tr(STR_BOOK_UNFAVORITE) : tr(STR_BOOK_FAVORITE),
                          tr(STR_COLLECTION_ADD), tr(STR_COLLECTION_MOVE), tr(STR_COLLECTION_REMOVE)};
  popup_.show(tr(STR_COLLECTIONS), labels, collection_ == collections::Finished ? 3 : 5, -1,
              [this, path, favorite](int option) {
                RenderLock lock(*this);
                switch (option) {
                  case 0:
                    lock.unlock();
                    onSelectBook(path);
                    return;
                  case 1:
                    if (!collections::setFavorite(path.c_str(), !favorite) || !store_.open())
                      fail();
                    else if (collection_ == collections::Favorites)
                      books(collection_);
                    break;
                  case 2:
                    chooseDestination(path, Transfer::Add);
                    break;
                  case 3:
                    chooseDestination(path, Transfer::Move);
                    break;
                  case 4: {
                    auto edit = makeUniqueNoThrow<collections::Selection>();
                    if (!edit || !edit->open(collection_, true) || !edit->set(path.c_str(), false) || !edit->save() ||
                        !store_.updateCount(collection_, edit->count()))
                      fail();
                    else {
                      edit->close();
                      books(collection_);
                    }
                    break;
                  }
                }
                requestUpdate();
              });
  requestUpdate();
}
void CollectionsActivity::onBackButton() {
  RenderLock lock(*this);
  app.clearTapFlash();
  switch (view_) {
    case View::Catalog:
      setResult(ActivityResult(MenuResult{collectionui::ReturnHome}));
      finish();
      return;
    case View::Books:
      catalog();
      return;
    case View::Destination:
      if (!origin_) {
        finish();
        return;
      }
      incomingBook_.clear();
      books(origin_);
      return;
    case View::Editor:
      if (directory_ != "/") {
        settleEditorFolder();
        const auto at = directory_.rfind('/');
        const auto parent = at ? directory_.substr(0, at) : "/";
        const bool restore = editorDepth_ && editorHistory_[editorDepth_ - 1].length == parent.size();
        const auto previous = restore ? editorHistory_[--editorDepth_] : EditorLocation{};
        changeDirectory(parent);
        if (restore) {
          nav.selected = previous.selected;
          nav.top = previous.top;
          nav.followOnBuild = false;
        }
        return;
      }
      if (!dirty_) {
        books(collection_);
        return;
      }
      const char* choices[] = {tr(STR_CANCEL), tr(STR_CONFIRM)};
      popup_.showMessage(tr(STR_COLLECTIONS), tr(STR_COLLECTION_DISCARD), choices, 2, -1, [this](int choice) {
        if (choice == 1) {
          RenderLock lock(*this);
          books(collection_);
        }
      });
      requestUpdate();
      return;
  }
}
void CollectionsActivity::buildScreen(UiScreen& hostScreen) {
  toybox::Screen screen(hostScreen.frame());
  pixelui::FontScope fonts(uiTarget, renderer, true);
  const char* title = view_ == View::Catalog       ? tr(STR_LIBRARY)
                      : view_ == View::Destination ? tr(STR_COLLECTION_PICK_TARGET)
                                                   : collectionName_.c_str();
  libraryui::header(screen, header_, title, false, {}, {}, 0, 0, view_ == View::Editor ? 0 : Command, grid_,
                    view_ == View::Books ? Options : 0);
  pixelui::begin(screen, view_ == View::Books ? 0 : 64);
  if (view_ == View::Catalog || view_ == View::Books) libraryui::filterTabs(screen, 1, Tab);
  switch (view_) {
    case View::Catalog:
    case View::Destination:
      buildCatalog(screen);
      break;
    case View::Books:
      buildBooks(screen);
      break;
    case View::Editor:
      buildEditor(screen);
      break;
  }
  const auto safe = screen.device().safeRect();
  const auto button = fui::makeRect(safe.x + 10, safe.bottom() - 56, safe.width - 20, 48);
  if (view_ == View::Editor)
    dashboard::button(screen, button, tr(STR_COLLECTION_SAVE), Command, Save, true);
  else if (view_ == View::Catalog || view_ == View::Destination)
    dashboard::button(screen, button, tr(STR_COLLECTION_NEW), Command, New, true);
}
void CollectionsActivity::buildCatalog(toybox::Screen& screen) {
  const auto grid = collectionui::layout(screen.device(), screen.body());
  const int capacity = grid_ ? grid.capacity() : std::max(1, (screen.body().height - 56) / collectionui::ListPitch);
  const int first = std::clamp(int(nav.selected), 0, std::max(0, listCount() - 1)) / capacity * capacity;
  nav.top = first;
  nav.visibleRows = nav.drawnRows = capacity;
  nav.drawnCount = listCount();
  nav.followOnBuild = false;
  for (int n = 0; n < capacity && first + n < listCount(); ++n) {
    const unsigned row = first + n;
    const auto id = catalogId(row);
    const auto* value = store_.find(id);
    const char* count = "";
    char number[64];
    if (value && id != collections::Finished) {
      std::snprintf(number, sizeof(number), grid_ ? "%u" : tr(STR_COLLECTION_COUNT), unsigned(value->count));
      count = number;
    }
    if (grid_)
      collectionui::card(screen, grid.card(n), nameFor(id), count, value && value->favorite, ACTION_ROW, row,
                         view_ == View::Catalog ? Options : 0);
    else {
      const auto body = screen.body();
      const auto box =
          fui::makeRect(body.x, body.y + n * collectionui::ListPitch, body.width, collectionui::ListHeight);
      collectionui::row(screen, box, nameFor(id), count, value && value->favorite, ACTION_ROW, row,
                        view_ == View::Catalog ? Options : 0);
    }
  }
  const int pages = std::max(1, (listCount() + capacity - 1) / capacity);
  pageui::build(screen, grid.pager, first / capacity + 1, pages, Page, first - capacity, Page, first + capacity);
}
void CollectionsActivity::buildRows(toybox::Screen& screen) {
  fui::ListProps props;
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch | fui::InputLongPress;
  props.checkboxCheckmark = true;
  uiTarget.setFont(4, toybox::kBookListFontId);
  libraryui::styleBookRows(props, screen.target());
  if (chrome::lightHeader) libraryui::lightRows(props);
  props.rowHeight = std::max(int(props.rowHeight), 2 * int(screen.target().lineHeight(4)) + 2 * props.rowPaddingY);
  props.selectedIndex = -1;
  syncListViewport(screen, props);
  props.partialTrailingRow = false;
  props.selectionMarker = fui::SelectionMarker::None;
  props.rowStyles.selected = props.rowStyles.focused = props.rowStyles.active = props.rowStyles.normal;
  const int first = props.topIndex;
  const int count = std::min({10, int(props.count) - first, std::max(1, int(nav.visibleRows) + 1)});
  for (int n = 0; n < count; ++n) {
    auto& item = items_[n];
    item = {};
    titles_[n].clear();
    values_[n].clear();
    item.actionValue = first + n;
    if (bookPath(first + n, path_)) {
      basename(path_, titles_[n]);
      const auto slash = path_.rfind('/');
      const auto dot = path_.rfind('.');
      if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        values_[n].assign(path_, dot + 1, std::string::npos);
      item.icon = listIconFor(UITheme::getFileIcon(path_), 32);
    }
    item.label = titles_[n].c_str();
    item.value = values_[n].c_str();
    item.labelBold = false;
  }
  props.items = items_;
  props.itemsWindowFirst = first;
  props.itemsWindowCount = count;
  pixelui::list(screen, props);
}
void CollectionsActivity::buildEditor(toybox::Screen& screen) {
  char selected[64];
  std::snprintf(selected, sizeof(selected), tr(STR_COLLECTION_SELECTED), selection_.count());
  pixelui::text(screen.target(), screen.takeTop(34, 6), selected, 4);
  const auto location = screen.takeTop(36, 6);
  title_ = directory_;
  fui::TextStyle pathStyle;
  pathStyle.font = 3;
  if (screen.target().measureText(3, title_.c_str(), pathStyle).width > location.width) {
    const auto slash = directory_.rfind('/');
    title_ = "..." + directory_.substr(slash);
  }
  pixelui::text(screen.target(), location, title_.c_str(), 3);
  uiTarget.setFont(4, toybox::kBookListFontId);
  const auto viewport = collectionui::editorViewport(screen, nav, listCount());
  const int first = viewport.first, count = viewport.count, height = viewport.height;
  const auto rows = viewport.rows;
  if (editorFirst_ != first || editorCount_ != count) {
    cancelEditorScan();
    editorFirst_ = first;
    editorFilesValid_ = editorFilesChecked_ = editorUnavailable_ = 0;
  }
  editorCount_ = count;
  editorKnownRows_ = 0;
  bool needsScan = editorScanRow_ >= 0;
  if (!chrome::lightHeader && count) dashboard::frame(screen, rows);
  for (int n = 0; n < count; ++n) {
    bool directory = false, checked = false;
    if (!folder_.entry(first + n, titles_[n], directory)) continue;
    path_ = directory_;
    if (path_.back() != '/') path_ += '/';
    path_ += titles_[n];
    collections::FolderSummary summary;
    if (directory) {
      if (!editorFolders_.get(path_.c_str(), summary)) {
        if (!selection_.mayContainSelected(path_.c_str())) {
          editorFolders_.put(path_.c_str(), summary);
          editorKnownRows_ |= 1u << n;
        } else if (!(editorUnavailable_ & (1u << n)))
          needsScan = true;
      } else
        editorKnownRows_ |= 1u << n;
    } else if (editorFilesValid_ & (1u << n))
      checked = (editorFilesChecked_ & (1u << n)) != 0;
    else {
      if (!selection_.has(path_.c_str(), checked)) {
        fail();
        break;
      }
      editorFilesValid_ |= 1u << n;
      if (checked) editorFilesChecked_ |= 1u << n;
    }
    values_[n].clear();
    if (!directory) editorKnownRows_ |= 1u << n;
    if (!directory) {
      const auto dot = titles_[n].rfind('.');
      if (dot != std::string::npos) {
        values_[n].assign(titles_[n], dot, std::string::npos);
        titles_[n].resize(dot);
      }
    }
    collectionui::editorRow(screen, fui::makeRect(rows.x, rows.y + n * height, rows.width, height), titles_[n].c_str(),
                            directory, checked || summary.state == collections::FolderSelection::All,
                            summary.state == collections::FolderSelection::Partial, n + 1 == count, ACTION_ROW, Options,
                            first + n, values_[n].c_str(), toybox::interfaceFontStyle() == menufont::Style::Pixel);
  }
  editorScanPending_ = needsScan;
  fui::drawListScrollIndicator(screen.target(), screen.body(), listCount(), count, first);
}

void CollectionsActivity::drawCover(toybox::Screen& screen, fui::Rect card, const std::string& path, int slot,
                                    bool defer) {
  const auto box = card.inset({2, 2, 2, 2});
  libraryui::coverFrame(screen.target(), card);
  const bool xtc = FsHelpers::hasXtcExtension(path), epub = FsHelpers::hasEpubExtension(path);
  char thumb[112];
  const int height = libraryui::preparationCoverHeight(screen.device(), false);
  bool drawn = false;
  if (!defer && (xtc || epub)) {
    const int heights[] = {height, libraryui::preparationCoverHeight(screen.device(), false, libraryui::kFooterHeight)};
    for (const int cachedHeight : heights) {
      std::snprintf(thumb, sizeof(thumb), "/.crosspoint/%s_%lu/thumb_%s%d.bmp", xtc ? "xtc" : "epub",
                    static_cast<unsigned long>(std::hash<std::string>{}(path)),
                    xtc && SETTINGS.xtcCoverRotation ? "ccw3_" : "", cachedHeight);
      if (!Storage.openFileForRead("COLL", thumb, coverFile_)) continue;
      if (cover_.parseHeaders() == BmpReaderError::Ok)
        drawn = GUI.drawCoverThumbFill(renderer, cover_, Rect{box.x, box.y, box.width, box.height});
      coverFile_.close();
      if (drawn) break;
    }
  }
  if (!drawn) {
    const bool waiting =
        (xtc || epub) && (defer || !(attempted_ & (1u << slot)) || (pending_.load() && pendingCover_ == path));
    if (waiting) {
      GUI.drawCoverPlaceholder(renderer, Rect{card.x, card.y, card.width, card.height});
    } else {
      dashboard::icon(screen, fui::makeRect(box.x + (box.width - 40) / 2, box.y + 12, 40, 40), icon_book_32);
      fui::TextStyle text;
      text.font = 3;
      text.maxLines = 3;
      text.align = fui::TextAlign::Center;
      basename(path, title_);
      screen.target().text(fui::makeRect(box.x + 6, box.y + 58, box.width - 12, box.height - 64), title_.c_str(), text);
    }
    if (xtc || epub) unresolvedCovers_ |= waiting;
    if (!defer && (xtc || epub) && !(attempted_ & (1u << slot)) && !pending_.load()) {
      pendingCover_ = path;
      coverHeight_ = height;
      attempted_ |= 1u << slot;
      pending_ = true;
      coverPaint_ = millis();
    }
  }
}
void CollectionsActivity::buildBooks(toybox::Screen& screen) {
  if (bookPhase_ != BookPhase::Ready) {
    const auto area = libraryui::entryBackdrop(renderer, screen.target(), screen.device(), screen.body(), grid_,
                                               bookCount_, nav.selected);
    if (grid_) libraryui::loadingNotice(screen.target(), area, tr(STR_LIBRARY_LOADING));
    return;
  }
  if (!bookCount_) {
    screen.centeredText(tr(STR_COLLECTION_EMPTY));
    return;
  }
  if (!grid_) {
    buildRows(screen);
    return;
  }
  const auto grid = libraryui::gridLayout(screen.device(), screen.body());
  const int cap = grid.capacity();
  const int first = std::clamp(int(nav.selected), 0, std::max(0, listCount() - 1)) / cap * cap;
  if (coverPage_ != first) {
    coverPage_ = first;
    attempted_ = 0;
    pending_ = false;
    pendingCover_.clear();
    coverWork_ = false;
    coverPresentation_ = CoverPresentation::Unannounced;
  }
  nav.top = first;
  nav.visibleRows = nav.drawnRows = cap;
  nav.drawnCount = listCount();
  nav.followOnBuild = false;
  const bool defer =
      coverPresentation_ == CoverPresentation::Unannounced || coverPresentation_ == CoverPresentation::Notice;
  unresolvedCovers_ = false;
  for (int n = 0; n < cap && first + n < listCount(); ++n) {
    if (!bookPath(first + n, path_)) continue;
    const auto box = grid.card(n);
    drawCover(screen, box, path_, n, defer);
    screen.frame().hit(box, ACTION_ROW, first + n, fui::InputTouch | fui::InputLongPress);
  }
  pageui::build(screen, grid.pager, first / cap + 1, (listCount() + cap - 1) / cap, Page, first - cap, Page,
                first + cap);
  coverPresentation_ = unresolvedCovers_ ? (defer ? CoverPresentation::Notice : CoverPresentation::Loading)
                                         : CoverPresentation::Complete;
  if (coverPresentation_ != CoverPresentation::Complete)
    libraryui::loadingNotice(screen.target(), grid.area, tr(STR_LIBRARY_LOADING_COVERS));
}
bool CollectionsActivity::continueEditorScan(void* context) {
  auto& self = *static_cast<CollectionsActivity*>(context);
  return !self.editorInterrupted_ && self.view_ == View::Editor && activityManager.idleWorkIsCurrent(self) &&
         self.editorSource_ == editorSourceRevision();
}
void CollectionsActivity::inspectEditorFolders() {
  if (editorScanRow_ >= 0) {
    auto state = collections::FolderSelection::None;
    const auto result = editorScan_.step(selection_, state, 4, &CollectionsActivity::continueEditorScan, this);
    if (result == collections::FolderScanResult::Pending) return;
    if (!continueEditorScan(this)) {
      editorScan_.close();
      editorScanRow_ = -1;
      editorScanPath_.clear();
      return;
    }
    if (result == collections::FolderScanResult::Error) {
      LOG_DBG("COLL", "Folder state unavailable");
      const int slot = editorScanRow_ - editorFirst_;
      if (slot >= 0 && slot < 10) editorUnavailable_ |= 1u << slot;
    } else {
      editorFolders_.put(editorScanPath_.c_str(), editorScan_.summary());
      const int slot = editorScanRow_ - editorFirst_;
      if (slot >= 0 && slot < 10) editorKnownRows_ |= 1u << slot;
      if (state != collections::FolderSelection::None) {
        if (!editorNeedsPaint_) editorPaintAt_ = millis();
        editorNeedsPaint_ = true;
      }
    }
    editorScanRow_ = -1;
    editorScanPath_.clear();
  }
  for (int n = 0; n < editorCount_; ++n) {
    if ((editorUnavailable_ | editorKnownRows_) & (1u << n)) continue;
    bool directory = false;
    if (!folder_.entry(editorFirst_ + n, entry_, directory) || !directory) continue;
    path_ = directory_;
    if (path_.back() != '/') path_ += '/';
    path_ += entry_;
    collections::FolderSummary summary;
    if (editorFolders_.get(path_.c_str(), summary)) {
      editorKnownRows_ |= 1u << n;
      continue;
    }
    if (!selection_.mayContainSelected(path_.c_str())) {
      editorFolders_.put(path_.c_str(), summary);
      editorKnownRows_ |= 1u << n;
      continue;
    }
    if (!editorScan_.start(path_, collections::FolderScanMode::Inspect, &knownEditorFolder, &editorFolders_)) {
      editorUnavailable_ |= 1u << n;
      LOG_DBG("COLL", "Cannot inspect folder: %s", path_.c_str());
      continue;
    }
    editorScanPath_ = path_;
    editorScanRow_ = editorFirst_ + n;
    return;
  }
  editorScanPending_ = false;
}
void CollectionsActivity::runIdleWork() {
  if (view_ == View::Books) {
    if (!activityManager.idleWorkIsCurrent(*this)) return;
    if (bookPhase_ == BookPhase::Preparing) {
      if (!loadBooks()) catalog();
    } else if (coverWork_.exchange(false) && pending_.exchange(false)) {
      generatePendingCover();
      requestUpdate();
    }
    return;
  }
  if (!continueEditorScan(this)) {
    editorScan_.close();
    editorScanRow_ = -1;
    editorScanPath_.clear();
    return;
  }
  inspectEditorFolders();
  if (editorNeedsPaint_ && (!editorScanPending_ || millis() - editorPaintAt_ >= EditorPaintBatchMs)) {
    editorNeedsPaint_ = false;
    requestUpdate();
  }
}
bool CollectionsActivity::handleCustomInput() {
  if (view_ == View::Books && bookPhase_ != BookPhase::Ready) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
        mappedInput.wasSwipe() == MappedInputManager::SwipeDir::Right)
      onBackButton();
    else {
      const auto routed = UiAppHost::routeTouch(mappedInput, true);
      if (routed.routed && app.invalidated()) requestUpdate();
      if (bookPhase_ == BookPhase::Preparing) activityManager.requestIdleWork(*this);
    }
    return true;
  }
  if (view_ == View::Editor) {
    int x = 0, y = 0;
    bool input = mappedInput.wasSwipe() != MappedInputManager::SwipeDir::None || mappedInput.wasScreenTapped(x, y) ||
                 mappedInput.wasScreenTouchPressed(x, y) || mappedInput.isScreenTouchHeld(x, y) ||
                 mappedInput.wasScreenTouchReleased();
    for (const auto key :
         {MappedInputManager::Button::Back, MappedInputManager::Button::Confirm, MappedInputManager::Button::NavNext,
          MappedInputManager::Button::NavPrevious, MappedInputManager::Button::Left, MappedInputManager::Button::Right})
      input |= mappedInput.isPressed(key) || mappedInput.wasPressed(key) || mappedInput.wasReleased(key);
    if (input) {
      editorInterrupted_ = true;
      editorInputAt_ = millis();
    }
    RenderLock lock(input ? RenderLock::Mode::Blocking : RenderLock::Mode::Try);
    if (lock.ownsLock()) {
      const auto revision = editorSourceRevision();
      if (revision != editorSource_) {
        cancelEditorScan();
        editorFolders_.clear();
        editorDepth_ = 0;
        editorSource_ = revision;
        changeDirectory(directory_);
        return true;
      }
      if (!input && editorScanPending_ && routingReady() && !popup_.isActive() &&
          millis() - editorInputAt_ >= EditorIdleMs) {
        editorInterrupted_ = false;
        activityManager.requestIdleWork(*this);
      }
    }
  }
  if (popup_.handleInput(mappedInput, [this] { requestUpdate(); })) return true;
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Right &&
      (view_ == View::Catalog || view_ == View::Destination || (view_ == View::Books && nav.top == 0))) {
    onBackButton();
    return true;
  }
  if ((view_ == View::Catalog || view_ == View::Destination || (grid_ && view_ == View::Books)) &&
      (swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Right ||
       swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down)) {
    const bool next = swipe == MappedInputManager::SwipeDir::Left || swipe == MappedInputManager::SwipeDir::Up;
    RenderLock lock(*this);
    const int destination =
        std::clamp(nav.top + (next ? nav.visibleRows : -nav.visibleRows), 0, std::max(0, listCount() - 1));
    if (destination != nav.top) {
      nav.requestSelection(destination);
      changed();
    }
    return true;
  }
  if (pending_ && millis() - coverPaint_ > 250) {
    coverWork_ = true;
    activityManager.requestIdleWork(*this);
  }
  return false;
}
void CollectionsActivity::render(RenderLock&& lock) {
  if (popup_.processRender(renderer, mappedInput)) return;
  UiListActivity::render(std::move(lock));
  if (view_ == View::Books && bookPhase_ == BookPhase::Shell) {
    bookPhase_ = BookPhase::Preparing;
    activityManager.requestIdleWork(*this);
  } else if (view_ == View::Books && coverPresentation_ == CoverPresentation::Notice) {
    coverPresentation_ = CoverPresentation::Loading;
    requestUpdate();
  }
}
void CollectionsActivity::generatePendingCover() {
  if (FsHelpers::hasEpubExtension(pendingCover_)) {
    auto epub = makeUniqueNoThrow<Epub>(pendingCover_, "/.crosspoint");
    if (epub)
      epub->generateThumbBmpFromSource(coverHeight_);
    else
      LOG_ERR("COLL", "OOM: EPUB cover");
  } else if (FsHelpers::hasXtcExtension(pendingCover_)) {
    auto xtc = makeUniqueNoThrow<Xtc>(pendingCover_, "/.crosspoint", SETTINGS.xtcCoverRotation != 0);
    if (xtc && xtc->load()) xtc->generateThumbBmp(coverHeight_);
  }
  pendingCover_.clear();
}
