#include "BookOrbitSettingsActivity.h"

#include <ContentVisibility.h>
#include <FsHelpers.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "BookOrbit.h"
#include "BookOrbitCatalogActivity.h"
#include "BookOrbitPending.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "activities/RenderLock.h"
#include "activities/UiTabListActivity.h"
#include "activities/reader/KOReaderSyncActivity.h"
#include "activities/settings/KOReaderAuthActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "apps_local/library/LibraryMenu.h"
#include "apps_local/ui/PixelMenu.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "apps_local/ui/SettingsMenuStyle.h"
#include "apps_local/ui/SettingsOptionIcons.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace bookorbit {
namespace {
namespace fui = freeink::ui;

class SyncPickerActivity final : public UiTabListActivity {
 public:
  SyncPickerActivity(GfxRenderer& renderer, MappedInputManager& input)
      : UiTabListActivity("BookOrbitSyncPicker", renderer, input) {
    setInterfaceFontSection(menufont::Section::Files);
  }

  void onEnter() override {
    recentBooks = &RECENT_BOOKS.getBooks();
    const auto& lastPath = APP_STATE.openEpubPath;
    if (FsHelpers::hasEpubExtension(std::string_view(lastPath)) && !content::hiddenContent(lastPath) &&
        Storage.exists(lastPath.c_str())) {
      currentPath = lastPath;
    } else if (lastPath.empty() && !recentBooks->empty()) {
      const auto& lastBook = recentBooks->front();
      if (FsHelpers::hasEpubExtension(std::string_view(lastBook.path)) && !RecentBooksStore::isMissing(lastBook))
        currentPath = lastBook.path;
    }
    refreshPending();

    UiTabListActivity::onEnter();
    if (listCount() > 0) moveRingTo(1);
    app.on(
        ACTION_TAB_USER, [](const fui::ActionEvent&, void* user) { static_cast<SyncPickerActivity*>(user)->sync(); },
        this);
  }

 private:
  const std::vector<RecentBook>* recentBooks = nullptr;
  uint8_t indices[RecentBooksStore::MAX_RECENT_BOOKS]{};
  int recentCount = 0;
  bool recentLoaded = false;
  int tab = 0;
  std::string currentPath;
  PendingBooks pending;
  bool currentPending = false;
  size_t pendingOffset = 0;

  int tabCount() const override { return 2; }
  int activeTab() const override { return tab; }
  const char* tabLabel(int index) const override {
    return index == 0 ? tr(STR_BOOKORBIT_IN_READING) : tr(STR_LIBRARY_TAB_RECENT);
  }
  void stepTab(int) override { onTabAction(1 - tab); }

  void onTabAction(int index) override {
    if (tab == index) return;
    if (index == 1 && !recentLoaded) {
      for (size_t i = 0; i < recentBooks->size() && i < std::size(indices); ++i) {
        const auto& book = (*recentBooks)[i];
        if (FsHelpers::hasEpubExtension(std::string_view(book.path)) && !RecentBooksStore::isMissing(book))
          indices[recentCount++] = static_cast<uint8_t>(i);
      }
      recentLoaded = true;
    }
    RenderLock lock;
    tab = index;
    if (listCount() > 0 && ringPos() == 0) activeNav().requestSelection(1);
    requestUpdate();
  }

  bool hasSeparateCurrent() const { return !currentPath.empty() && !currentPending; }
  int listCount() const override {
    return tab == 1 ? recentCount : static_cast<int>(pending.paths.size()) + hasSeparateCurrent();
  }

  void refreshPending() {
    auto updated = readPending(currentPath, pendingOffset);
    const size_t currentOnly = !updated.paths.empty() && updated.paths.front() == currentPath ? 1 : 0;
    if (pendingOffset && updated.paths.size() == currentOnly) {
      pendingOffset = 0;
      updated = readPending(currentPath);
    }
    RenderLock lock;
    pending = std::move(updated);
    currentPending = !pending.paths.empty() && pending.paths.front() == currentPath;
  }

  void drawChrome() override {
    pixelui::drawHeader(renderer, tr(STR_BOOKORBIT), I18N.getLanguage() != Language::EN, nullptr, false, true);
  }

  void activateIndex(int index) override { moveRingTo(index + 1); }

  bool handleButtons() override {
    if (!mappedInput.wasReleased(MappedInputManager::Button::Confirm)) return UiListActivity::handleButtons();
    if (ringPos() == 0) {
      stepTab(1);
    } else {
      sync();
    }
    return true;
  }

  void sync() {
    if (!Store::get().authenticated()) return;
    if (tab == 1) {
      const int index = ringPos() - 1;
      if (index < 0 || index >= recentCount) return;
      startSync(makeUniqueNoThrow<KOReaderSyncActivity>(renderer, mappedInput, (*recentBooks)[indices[index]].path));
      return;
    }

    if (pending.paths.empty()) return;
    std::vector<BatchBook> books;
    books.reserve(pending.paths.size());
    const size_t nextOffset = pending.hasMore ? pendingOffset + pending.paths.size() - currentPending : 0;
    {
      RenderLock lock;
      for (auto& path : pending.paths) books.push_back({std::move(path)});
      pending.paths.clear();
      currentPending = false;
    }
    auto syncActivity = makeUniqueNoThrow<KOReaderSyncActivity>(renderer, mappedInput, std::move(books));
    if (syncActivity)
      pendingOffset = nextOffset;
    else
      refreshPending();
    startSync(std::move(syncActivity));
  }

  void startSync(std::unique_ptr<KOReaderSyncActivity> syncActivity) {
    if (!syncActivity) {
      LOG_ERR("ORBIT", "OOM: sync picker");
      return;
    }
    app.clearTapFlash();
    startActivityForResult(std::move(syncActivity), [this](const ActivityResult&) { refreshPending(); });
  }

  const char* titleFor(const std::string& path) const {
    for (const auto& book : *recentBooks) {
      if (book.path == path && !book.title.empty()) return book.title.c_str();
    }
    return path.c_str() + (path.find_last_of('/') + 1);
  }

  void provideRow(uint16_t index, fui::ListItem& row) const {
    if (tab == 1) {
      const auto& book = (*recentBooks)[indices[index]];
      row.label = book.title.empty() ? book.path.c_str() + (book.path.find_last_of('/') + 1) : book.title.c_str();
    } else {
      const bool separateCurrent = hasSeparateCurrent();
      const bool current = index == 0 && (separateCurrent || currentPending);
      const auto& path = separateCurrent && current ? currentPath : pending.paths[index - separateCurrent];
      row.label = titleFor(path);
      row.labelBold = current;
      row.subtitle = current ? (currentPending ? tr(STR_BOOKORBIT_CURRENT_PENDING) : tr(STR_BOOKORBIT_CURRENT_BOOK))
                             : tr(STR_BOOKORBIT_PENDING_BOOK);
    }
    row.icon = listIconFor(UIIcon::Book, 32);
    row.actionValue = index;
  }

  void buildScreen(UiScreen& screen) override {
    pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
    const auto& metrics = UITheme::getInstance().getMetrics();
    libraryui::beginFileRows(screen, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);
    const fui::TabItem tabs[] = {fui::tabItem(0, tab == 0, true, tabLabel(0)),
                                 fui::tabItem(1, tab == 1, true, tabLabel(1))};
    pixelui::tabs(screen, tabs, 2, ACTION_TAB, pixelui::kSmall);

    fui::ButtonProps button;
    button.label = tab == 0 ? tr(STR_BOOKORBIT_SYNC_PENDING) : tr(STR_BOOKORBIT_SYNC_BOOK);
    button.enabled = tab == 0 ? !pending.paths.empty() : ringPos() > 0 && ringPos() <= recentCount;
    button.action = ACTION_TAB_USER;
    button.inputMask = fui::InputTouch;
    button.text.font = pixelui::kSmall;
    button.text.align = fui::TextAlign::Center;
    button.text.maxLines = 2;
    button.styles = pixelui::buttonStyles();
    button.styles.normal = button.styles.selected;
    button.radius = chrome::lightHeader ? lightui::radius() : 0;
    screen.button(button, screen.takeBottom(58, 10));

    const char* notice = nullptr;
    if (tab == 0) {
      if (pending.readError)
        notice = tr(STR_BOOKORBIT_PENDING_READ_ERROR);
      else if (pending.hasMore)
        notice = tr(STR_BOOKORBIT_MORE_PENDING);
      else if (pending.paths.empty())
        notice = tr(STR_BOOKORBIT_NO_PENDING);
    } else if (!recentCount) {
      notice = tr(STR_BOOKORBIT_NO_RECENT_EPUB);
    }
    if (notice)
      pixelui::text(screen.target(), screen.takeTop(42, 6), notice, pixelui::kSmall, fui::Color::Black,
                    fui::TextAlign::Center);
    if (!listCount()) return;

    fui::ListProps props;
    props.count = listCount();
    props.action = ACTION_ROW;
    props.inputMask = fui::InputTouch;
    props.rowProviderCtx = this;
    props.rowProvider = [](void* context, uint16_t index, fui::ListItem& row) {
      static_cast<const SyncPickerActivity*>(context)->provideRow(index, row);
    };
    uiTarget.setFont(4, toybox::kBookListFontId);
    libraryui::styleBookRows(props, screen.target());
    if (chrome::lightHeader) {
      const auto compactHeight = props.rowHeight;
      const auto compactPadding = props.rowPaddingY;
      libraryui::lightRows(props);
      props.rowHeight = compactHeight;
      props.rowPaddingY = compactPadding;
      props.rowStyles.selected.radius = lightui::radius();
      props.separatorPaint = fui::Paint::solid(fui::Color::Black);
      props.omitSelectedSeparators = true;
    }
    libraryui::fitPortraitRows(props, screen.device(), screen.body());
    props.rowStyles.selected.background = fui::Paint::solid(fui::Color::Black);
    props.rowStyles.selected.foreground = fui::Paint::solid(fui::Color::White);
    props.rowStyles.focused = props.rowStyles.active = props.rowStyles.selected;
    props.selectionMarker = fui::SelectionMarker::None;
    props.rtl =
        I18N.getLanguage() == Language::AR || I18N.getLanguage() == Language::FA || I18N.getLanguage() == Language::HE;
    syncTabListViewport(screen, props);
    props.partialTrailingRow = false;
    if (chrome::lightHeader)
      fui::list(screen.frame(), screen.body(), props);
    else
      pixelui::list(screen, props, false, false, 2);
  }
};
}  // namespace

SettingsActivity::SettingsActivity(GfxRenderer& renderer, MappedInputManager& input)
    : UiListActivity("BookOrbitSettings", renderer, input, false, true) {
  static constexpr StrId labels[] = {StrId::STR_SYNC_SERVER_URL,
                                     StrId::STR_USERNAME,
                                     StrId::STR_PASSWORD,
                                     StrId::STR_AUTHENTICATE,
                                     StrId::STR_BOOKORBIT_BROWSE,
                                     StrId::STR_OPDS_DOWNLOAD_FOLDER,
                                     StrId::STR_SYNC_BEHAVIOR,
                                     StrId::STR_BOOKORBIT_SYNC_SLEEP,
                                     StrId::STR_BOOKORBIT_SYNC_WAKE,
                                     StrId::STR_BOOKORBIT_SYNC_NOW};
  static constexpr fui::BitmapRef icons[] = {
      settingsicons::Servers, settingsicons::BookInfo, settingsicons::Lock,  settingsicons::Wifi, settingsicons::Books,
      settingsrowart::Folder, settingsicons::Sliders,  settingsrowart::Moon, settingsicons::Wake, settingsicons::Sync};
  static_assert(std::size(labels) == ROW_COUNT && std::size(icons) == ROW_COUNT);
  for (int i = 0; i < ROW_COUNT; ++i) {
    rows[i].label = I18N.get(labels[i]);
    rows[i].icon = icons[i];
    rows[i].actionValue = static_cast<int16_t>(i);
  }
  rows[static_cast<int>(Row::Server)].sectionHeading = tr(STR_READING_SYNC_ACCOUNT);
  rows[static_cast<int>(Row::Catalog)].sectionHeading = tr(STR_LIBRARY);
  rows[static_cast<int>(Row::SyncBehavior)].sectionHeading = tr(STR_SETTINGS_SYNC_HELP);
}

const char* SettingsActivity::headerTitle() const { return tr(STR_BOOKORBIT); }

void SettingsActivity::activateIndex(const int index) {
  auto& settings = Store::get();
  app.clearTapFlash();
  const auto row = static_cast<Row>(index);
  switch (row) {
    case Row::Server:
    case Row::Username:
    case Row::Password:
    case Row::DownloadFolder: {
      const auto& value = row == Row::Server     ? settings.url
                          : row == Row::Username ? settings.username
                          : row == Row::Password ? settings.password
                                                 : settings.downloadFolder;
      const auto type = row == Row::Server     ? InputType::Url
                        : row == Row::Password ? InputType::Password
                                               : InputType::Text;
      if (auto entry = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, rows[index].label, value,
                                                                row == Row::Server           ? 128
                                                                : row == Row::DownloadFolder ? 63
                                                                                             : 64,
                                                                type)) {
        if (row == Row::Password) entry->setPasswordVisible(true);
        startActivityForResult(std::move(entry), [this, row](const ActivityResult& result) {
          if (result.isCancelled) return;
          const auto& edited = std::get<KeyboardResult>(result.data).text;
          auto& settings = Store::get();
          bool valid = true;
          {
            RenderLock lock(*this);
            if (row == Row::Server)
              settings.setUrl(edited);
            else if (row == Row::DownloadFolder)
              invalidFolder = !(valid = settings.setDownloadFolder(edited));
            else {
              auto& credential = row == Row::Username ? settings.username : settings.password;
              if (credential != edited) settings.setAuthenticated(false);
              credential = edited;
            }
          }
          if (valid) settings.saveToFile(true);
          requestUpdate();
        });
      } else {
        LOG_ERR("ORBIT", "OOM: settings keyboard");
      }
      break;
    }
    case Row::Authenticate:
      if (!settings.configured()) break;
      if (auto auth =
              makeUniqueNoThrow<KOReaderAuthActivity>(renderer, mappedInput, KOReaderAuthActivity::Mode::BOOK_ORBIT))
        startActivityForResult(std::move(auth), [](const ActivityResult&) {});
      else
        LOG_ERR("ORBIT", "OOM: authentication");
      break;
    case Row::SyncBehavior:
    case Row::SyncOnSleep:
    case Row::SyncOnWake: {
      {
        RenderLock lock(*this);
        auto& choice = row == Row::SyncBehavior  ? settings.smartSync
                       : row == Row::SyncOnSleep ? settings.syncOnSleep
                                                 : settings.syncOnWake;
        choice = !choice;
      }
      settings.saveToFile(true);
      requestUpdate();
      break;
    }
    case Row::Catalog:
      if (!settings.configured()) break;
      if (auto catalog = makeUniqueNoThrow<BookOrbitCatalogActivity>(renderer, mappedInput, settings.endpoint(),
                                                                     settings.downloadFolder))
        startActivityForResult(std::move(catalog), [](const ActivityResult&) {});
      else
        LOG_ERR("ORBIT", "OOM: BookOrbit catalogue");
      break;
    case Row::SyncNow:
      if (!settings.authenticated()) break;
      if (auto picker = makeUniqueNoThrow<SyncPickerActivity>(renderer, mappedInput))
        startActivityForResult(std::move(picker), [](const ActivityResult&) {});
      else
        LOG_ERR("ORBIT", "OOM: recent book picker");
      break;
    case Row::Count:
      break;
  }
}

void SettingsActivity::buildScreen(UiScreen& screen) {
  const auto& settings = Store::get();
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  useCompactSettingsRows();
  pixelui::begin(screen, mappedInput.hasTouch() ? 0 : UITheme::getInstance().getMetrics().buttonHintsHeight);
  details[0] = settings.url.empty() ? tr(STR_NOT_SET) : settings.url;
  details[1] = settings.username.empty() ? tr(STR_NOT_SET) : settings.username;
  details[2] = settings.password.empty() ? tr(STR_NOT_SET) : "******";
  for (int i = 0; i < 3; ++i) rows[i].subtitle = details[i].c_str();
  rows[static_cast<int>(Row::Authenticate)].subtitle =
      settings.configured() ? (settings.authenticated() ? tr(STR_BOOKORBIT_READY) : nullptr)
                            : tr(STR_SET_CREDENTIALS_FIRST);
  rows[static_cast<int>(Row::Catalog)].subtitle = settings.configured() ? nullptr : tr(STR_SET_CREDENTIALS_FIRST);
  rows[static_cast<int>(Row::SyncNow)].subtitle =
      settings.authenticated() ? nullptr
                               : (settings.configured() ? tr(STR_AUTHENTICATE) : tr(STR_SET_CREDENTIALS_FIRST));
  rows[static_cast<int>(Row::DownloadFolder)].subtitle = invalidFolder ? tr(STR_BOOKORBIT_INVALID_FOLDER)
                                                         : settings.downloadFolder.empty()
                                                             ? tr(STR_OPDS_SD_ROOT)
                                                             : settings.downloadFolder.c_str();
  rows[static_cast<int>(Row::SyncBehavior)].subtitle = settings.smartSync ? tr(STR_SMART_SYNC) : tr(STR_ASK_EVERY_TIME);
  GUI.setCheckboxRow(rows[static_cast<int>(Row::SyncOnSleep)], settings.syncOnSleep);
  GUI.setCheckboxRow(rows[static_cast<int>(Row::SyncOnWake)], settings.syncOnWake);

  freeink::ui::ListProps props;
  props.items = rows;
  props.count = ROW_COUNT;
  props.action = ACTION_ROW;
  props.inputMask = freeink::ui::InputTouch;
  props.valueInset = 8;
  syncListViewport(screen, props);
  settingsui::drawRows(screen, props, false);
}
}  // namespace bookorbit
