#include "BookOrbitCatalogActivity.h"

#include <Arduino.h>
#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <KOReaderDocumentId.h>
#include <LibraryBuilder.h>
#include <LibraryFormat.h>
#include <Logging.h>
#include <Memory.h>
#include <WiFi.h>
#include <strings.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string_view>

#include "BookOrbit.h"
#include "MappedInputManager.h"
#include "apps_local/library/LibraryMenu.h"
#include "apps_local/ui/CatalogHeader.h"
#include "apps_local/ui/PixelMenu.h"
#include "apps_local/ui/PixelMenuTheme.h"
#include "components/CatalogScreens.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "components/icons/search32.h"
#include "util/BookCacheUtils.h"
#include "util/OpdsFilename.h"
#include "util/PluginHttp.h"
#include "util/StringUtils.h"

namespace bookorbit {
namespace fui = freeink::ui;

namespace {
constexpr size_t MAX_RESPONSE_BYTES = 128 * 1024;
constexpr size_t MAX_LABEL_BYTES = 240;

class CatalogAllocator final : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t size) override { return HalMemory::allocatePsram(size).release(); }
  void deallocate(void* p) override { HalMemory::PsramDeleter{}(static_cast<uint8_t*>(p)); }
  void* reallocate(void* p, size_t size) override { return HalMemory::reallocatePsram(p, size); }
};

std::string catalogLabel(std::string_view full) {
  size_t end = std::min(full.size(), MAX_LABEL_BYTES);
  while (end > 0 && end < full.size() && (static_cast<unsigned char>(full[end]) & 0xc0) == 0x80) --end;
  return std::string(full.substr(0, end));
}

bool isDocumentHash(std::string_view hash) {
  return hash.size() == 32 &&
         std::all_of(hash.begin(), hash.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

bool isPresentEpub(const std::string& path) {
  auto file = Storage.open(path.c_str());
  return file && !file.isDirectory() && file.fileSize() > 0;
}

// Only the configured download directory is scanned, with one scratch buffer.
template <typename Cancelled>
std::optional<std::string> findDownloadedEpub(const std::string& folder, const std::string& preferred,
                                              std::string_view hash, const uint64_t size, std::string_view tag,
                                              const Cancelled& cancelled) {
  constexpr size_t NAME_BYTES = 768;
  constexpr size_t CHUNK_BYTES = KOReaderDocumentId::CHUNK_SIZE;
  auto scratch = makeUniqueNoThrow<uint8_t[]>(NAME_BYTES + (hash.empty() ? 0 : CHUNK_BYTES));
  if (!scratch) {
    LOG_ERR("ORBIT", "OOM: catalogue verification");
    return std::nullopt;
  }
  auto* name = reinterpret_cast<char*>(scratch.get());
  auto* chunk = scratch.get() + NAME_BYTES;
  {
    auto file = Storage.open(preferred.c_str());
    if (file && !file.isDirectory() && file.fileSize() && (!size || file.fileSize64() == size)) {
      if (hash.empty()) return preferred;
      const auto local = KOReaderDocumentId::calculate(file, chunk, CHUNK_BYTES);
      if (local.empty()) return std::nullopt;
      if (local == hash) return preferred;
    }
  }

  auto directory = Storage.open(folder.empty() ? "/" : folder.c_str());
  if (!directory || !directory.isDirectory()) return std::nullopt;
  HalFile entry;
  bool error = false;
  const std::string_view preferredName(preferred.c_str() + (preferred.find_last_of('/') + 1));
  while (!cancelled() && directory.openNextFile(entry, error)) {
    if (entry.isDirectory() || !entry.fileSize() || (size && entry.fileSize64() != size)) continue;
    const auto count = entry.getName(name, NAME_BYTES);
    if (!count || count >= NAME_BYTES - 1) continue;
    const std::string_view filename(name, count);
    if (filename == preferredName || !FsHelpers::hasEpubExtension(filename)) continue;
    if (hash.empty()) {
      if (filename.ends_with(tag)) return folder + '/' + name;
    } else {
      const auto local = KOReaderDocumentId::calculate(entry, chunk, CHUNK_BYTES);
      if (local.empty()) return std::nullopt;
      if (local == hash) return folder + '/' + name;
    }
  }
  if (error || cancelled()) return std::nullopt;
  return std::string{};
}

// A receipt is only needed when BookOrbit omits the delivered fingerprint.
void readDeliveryReceipt(const std::string& path, std::string& hash, uint64_t& size) {
  HalFile file;
  if (!Storage.openFileForRead("ORBIT", path, file)) return;
  char data[54];
  const int count = file.read(data, sizeof(data));
  if (count <= 33 || count >= int(sizeof(data)) || data[32] != ' ') return;
  const std::string_view fingerprint(data, 32);
  if (!isDocumentHash(fingerprint)) return;
  uint64_t length = 0;
  const auto parsed = std::from_chars(data + 33, data + count, length);
  if (parsed.ec != std::errc{} || parsed.ptr != data + count || !length) return;
  hash = fingerprint;
  size = length;
}

void saveDeliveryReceipt(const std::string& path, const std::string& hash, uint64_t size) {
  const auto temporary = path + ".tmp";
  char data[54];
  const auto len = std::snprintf(data, sizeof(data), "%s %llu", hash.c_str(), static_cast<unsigned long long>(size));
  {
    HalFile file;
    if (!Storage.openFileForWrite("ORBIT", temporary, file) || file.write(data, len) != size_t(len) || !file.close()) {
      LOG_ERR("ORBIT", "Could not save delivery fingerprint");
      return;
    }
  }
  if (!Storage.replaceFile(temporary.c_str(), path.c_str())) LOG_ERR("ORBIT", "Could not install delivery fingerprint");
}

// Only JSON metadata goes through this bounded PSRAM buffer. Books stream directly to the card.
class CatalogResponse final : public Stream {
 public:
  explicit CatalogResponse(uint8_t* bytes) : bytes(bytes) {}
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* data, size_t len) override {
    if (len > MAX_RESPONSE_BYTES - length) return 0;
    std::memcpy(bytes + length, data, len);
    length += len;
    return len;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  size_t size() const { return length; }

 private:
  uint8_t* bytes;
  size_t length = 0;
};

}  // namespace

BookOrbitCatalogActivity::BookOrbitCatalogActivity(GfxRenderer& renderer, MappedInputManager& input,
                                                   KOReaderEndpoint account, std::string folder)
    : CatalogActivity("BookOrbitCatalog", renderer, input),
      apiUrl(std::move(account.baseUrl) + "/plugin/catalog"),
      folder(std::move(folder)) {
  headers.reserve(2);
  headers.emplace_back("x-auth-user", std::move(account.username));
  headers.emplace_back("x-auth-key", std::move(account.authKey));
}

void BookOrbitCatalogActivity::onEnter() {
  CatalogActivity::onEnter();
  app.on(
      ACTION_OPEN_BOOK,
      [](const fui::ActionEvent&, void* user) { static_cast<BookOrbitCatalogActivity*>(user)->openDownloadedBook(); },
      this);
  startedWifi = WiFi.getMode() == WIFI_MODE_NULL;
  state = State::CHECK_WIFI;
  statusMessage = tr(STR_CHECKING_WIFI);
  checkAndConnectWifi();
}

void BookOrbitCatalogActivity::onExit() {
  if (startedWifi && WiFi.getMode() == WIFI_MODE_STA)
    CatalogActivity::onExit();
  else
    Activity::onExit();
}

bool BookOrbitCatalogActivity::fetchJson(const std::string& url, const JsonDocument& filter, JsonDocument& result) {
  {
    RenderLock lock(*this);
    if (auto* cache = renderer.getFontCacheManager()) cache->releaseSdFontCaches();
  }
  if (ESP.getFreeHeap() < HttpDownloader::MIN_TLS_FREE_HEAP ||
      ESP.getMaxAllocHeap() < HttpDownloader::MIN_TLS_MAX_ALLOC) {
    fail(StrId::STR_MEMORY_ERROR);
    return false;
  }
  auto buffer = HalMemory::allocatePsram(MAX_RESPONSE_BYTES);
  if (!buffer) {
    LOG_ERR("ORBIT", "OOM: catalogue response");
    fail(StrId::STR_MEMORY_ERROR);
    return false;
  }
  CatalogResponse response(buffer.get());
  bool back = false, home = false;
  auto lastPoll = millis();
  const bool fetched = HttpDownloader::fetchMetadata(url, response, headers, [&] {
    const auto now = millis();
    if (now - lastPoll >= 50) {
      lastPoll = now;
      mappedInput.update(true);
      back = mappedInput.wasReleased(MappedInputManager::Button::Back);
      home = mappedInput.wasHomeGesture();
    }
    return back || home;
  });
  if (!fetched) {
    const int status = HttpDownloader::lastStatus();
    if (home)
      onGoHome();
    else if (back)
      onBackButton();
    else
      fail(status == 401 || status == 403 ? StrId::STR_AUTH_FAILED : StrId::STR_FETCH_FEED_FAILED);
    return false;
  }
  const auto error = deserializeJson(result, reinterpret_cast<const char*>(buffer.get()), response.size(),
                                     DeserializationOption::Filter(filter));
  if (!error) return true;
  LOG_ERR("ORBIT", "Catalogue JSON: %s", error.c_str());
  fail(StrId::STR_PARSE_FEED_FAILED);
  return false;
}

void BookOrbitCatalogActivity::startBrowse() {
  closeRouting();
  beginLoading();

  std::string url = apiUrl + "/books?format=epub&size=" + std::to_string(PAGE_SIZE) + "&page=" + std::to_string(page);
  if (!query.empty()) url += "&q=" + pluginhttp::urlEncodeQuery(query);
  CatalogAllocator allocator;
  JsonDocument filter, data(&allocator);
  filter["items"][0]["id"] = true;
  filter["items"][0]["title"] = true;
  filter["items"][0]["authors"][0] = true;
  filter["total"] = true;
  if (!fetchJson(url, filter, data)) return;
  if (!data["items"].is<JsonArrayConst>() || !data["total"].is<int64_t>()) {
    fail(StrId::STR_PARSE_FEED_FAILED);
    return;
  }

  {
    RenderLock lock(*this);
    rowCount = 0;
    selectedBookRow = -1;
    if (page > 1) rows[rowCount++] = {.label = tr(STR_PREV_PAGE)};
    int bookCount = 0;
    for (JsonObjectConst item : data["items"].as<JsonArrayConst>()) {
      if (bookCount == PAGE_SIZE) break;
      const auto id = item["id"] | int64_t{0};
      const char* title = item["title"] | "";
      if (id <= 0 || !*title) continue;
      auto& book = books[bookCount++];
      book = {id, catalogLabel(title), catalogLabel(item["authors"][0] | ""), {}, 0};
      rows[rowCount++] = {.label = book.title.c_str(), .icon = listIconFor(UIIcon::Book, 32)};
    }
    hasNextPage = static_cast<int64_t>(page) * PAGE_SIZE < data["total"].as<int64_t>();
    if (hasNextPage) rows[rowCount++] = {.label = tr(STR_NEXT_PAGE)};
    for (int i = 0; i < rowCount; ++i) rows[i].actionValue = i;
    nav.reset();
    state = State::BROWSING;
  }
  requestUpdate();
}

void BookOrbitCatalogActivity::performSearch(const std::string& text) {
  query = text;
  page = 1;
  startBrowse();
}

void BookOrbitCatalogActivity::activateIndex(const int index) {
  app.clearTapFlash();
  const bool hasPrevious = page > 1;
  const bool previous = hasPrevious && index == 0;
  if (previous || (hasNextPage && index == rowCount - 1)) {
    page += previous ? -1 : 1;
    startBrowse();
    return;
  }
  selectedBookRow = index;
  auto& book = bookAt(index);
  if (!book.downloadedPath.empty() && book.downloadedRevision == Storage.resourceRevision() &&
      isPresentEpub(book.downloadedPath)) {
    showDownloaded(book.downloadedPath);
    return;
  }
  if (!book.downloadedPath.empty()) {
    RenderLock lock(*this);
    book.downloadedPath.clear();
    rows[index].subtitle = nullptr;
  }
  downloadBook(book);
}

void BookOrbitCatalogActivity::downloadBook(Book& book) {
  closeRouting();
  beginLoading();
  const auto fallbackName = opdsBookFilename(book.author, book.title, OpdsFilenameFormat::AuthorTitle);
  std::string filename = folder + '/' + fallbackName;
  int64_t fileId = 0;
  uint64_t size = 0;
  std::string hash, contentVersion, variant;
  {
    CatalogAllocator allocator;
    JsonDocument filter, data(&allocator);
    filter["files"][0]["id"] = true;
    filter["files"][0]["format"] = true;
    filter["files"][0]["fileHash"] = true;
    filter["files"][0]["sizeBytes"] = true;
    filter["files"][0]["contentVersion"] = true;
    filter["files"][0]["downloadVariant"] = true;
    if (!fetchJson(apiUrl + "/books/" + std::to_string(book.id), filter, data)) return;
    for (JsonObjectConst file : data["files"].as<JsonArrayConst>()) {
      const char* format = file["format"] | "";
      if (strcasecmp(format, "epub") != 0 || (fileId = file["id"] | int64_t{0}) <= 0) continue;
      const std::string_view digest(file["fileHash"] | "");
      if (isDocumentHash(digest)) hash = digest;
      size = file["sizeBytes"] | uint64_t{0};
      contentVersion = file["contentVersion"] | "";
      variant = file["downloadVariant"] | "";
      break;
    }
  }
  if (fileId <= 0) {
    fail(StrId::STR_NO_FILES_FOUND);
    return;
  }
  if (!folder.empty() && !Storage.ensureDirectoryExists(folder.c_str())) {
    LOG_ERR("ORBIT", "BookOrbit download folder unavailable");
    fail(StrId::STR_DOWNLOAD_FAILED);
    return;
  }
  struct ScanInput {
    BookOrbitCatalogActivity& catalog;
    unsigned long lastPoll;
    bool stopped = false;
  } scan{*this, millis()};
  const auto cancelled = [&scan] {
    if (scan.stopped) return true;
    const auto now = millis();
    if (now - scan.lastPoll < 50) return false;
    scan.lastPoll = now;
    auto& self = scan.catalog;
    self.mappedInput.update(true);
    if (self.mappedInput.wasHomeGesture()) {
      scan.stopped = true;
      self.onGoHome();
    } else if (self.state == State::DOWNLOADING) {
      self.routeTouch(self.mappedInput);
      scan.stopped = self.cancelDownload || self.mappedInput.wasReleased(MappedInputManager::Button::Back);
      if (scan.stopped) self.finishDownload(HttpDownloader::ABORTED);
    } else if (self.mappedInput.wasReleased(MappedInputManager::Button::Back)) {
      scan.stopped = true;
      self.onBackButton();
    }
    return scan.stopped;
  };

  char tag[26]{};
  std::string receipt, stage;
  const bool staged = hash.empty();
  if (staged) {
    // Some delivered variants have no fingerprint until they are streamed.
    if (!checkedServerIdentity) {
      const auto identity = readServerIdentity(Store::get(), cancelled);
      if (scan.stopped) return;
      checkedServerIdentity = identity.status != IdentityStatus::Unavailable;
      if (identity.status == IdentityStatus::Verified) serverInstance = identity.instanceId;
    }
    const auto identity = (serverInstance.empty() ? apiUrl : serverInstance) + '\n' + headers.front().second + '\n' +
                          std::to_string(fileId) + '\n' + variant + '\n' + contentVersion;
    const auto key = library::clixPathHash(identity.data(), identity.size());
    std::snprintf(tag, sizeof(tag), " [%016llx].epub", static_cast<unsigned long long>(key));
    filename = folder + '/' + StringUtils::sanitizeFilename(book.title, 80) + tag;
    receipt = folder + "/.bookorbit" + tag + ".receipt";
    stage = folder + "/.bookorbit.received";
    if (Storage.exists(stage.c_str())) Storage.remove(stage.c_str());
    readDeliveryReceipt(receipt, hash, size);
  }
  auto existing = findDownloadedEpub(folder, filename, hash, size, tag, cancelled);
  if (scan.stopped) return;
  if (!existing) {
    LOG_ERR("ORBIT", "Cannot verify EPUBs in download folder");
    fail(StrId::STR_DOWNLOAD_FAILED);
    return;
  }
  if (!existing->empty()) {
    showDownloaded(std::move(*existing));
    return;
  }
  if (!staged && Storage.exists(filename.c_str()))
    filename = folder + '/' + StringUtils::sanitizeFilename(book.title, 80) + " [" + hash + "].epub";
  if (Storage.exists(filename.c_str())) {
    fail(StrId::STR_DOWNLOAD_FAILED);
    return;
  }

  // Compare the actual delivery before publishing a hashless EPUB. This also
  // recognizes copies made before filenames carried a delivery identity.
  const auto& destination = staged ? stage : filename;
  beginDownload(book.title);
  const auto result =
      downloadFile(apiUrl + "/files/" + std::to_string(fileId) + "/download", destination, {}, {}, headers,
                   staged ? HttpDownloader::ExistingFile::Replace : HttpDownloader::ExistingFile::Keep);
  if (result == HttpDownloader::UNAUTHORIZED) {
    fail(StrId::STR_AUTH_FAILED);
    return;
  }
  if (result == HttpDownloader::OK) {
    if (staged) {
      const auto deliveredHash = KOReaderDocumentId::calculate(destination);
      uint64_t deliveredSize = 0;
      {
        auto file = Storage.open(destination.c_str());
        if (file) deliveredSize = file.fileSize64();
      }
      if (!deliveredHash.empty() && deliveredSize)
        existing = findDownloadedEpub(folder, filename, deliveredHash, deliveredSize, {}, cancelled);
      else
        existing.reset();
      if (scan.stopped || !existing || !existing->empty()) {
        Storage.remove(destination.c_str());
        if (scan.stopped) return;
        if (!existing)
          fail(StrId::STR_DOWNLOAD_FAILED);
        else {
          saveDeliveryReceipt(receipt, deliveredHash, deliveredSize);
          showDownloaded(std::move(*existing));
        }
        return;
      }
      if (!Storage.rename(destination.c_str(), filename.c_str())) {
        Storage.remove(destination.c_str());
        fail(StrId::STR_DOWNLOAD_FAILED);
        return;
      }
      saveDeliveryReceipt(receipt, deliveredHash, deliveredSize);
    }
    clearBookCache(filename);
    library::markLibraryIndexDirty();
    showDownloaded(std::move(filename));
    return;
  }
  finishDownload(result);
}

void BookOrbitCatalogActivity::showDownloaded(std::string path) {
  {
    RenderLock lock(*this);
    auto& book = bookAt(selectedBookRow);
    book.downloadedPath = std::move(path);
    book.downloadedRevision = Storage.resourceRevision();
    rows[selectedBookRow].subtitle = tr(STR_BOOKORBIT_DOWNLOADED);
    state = State::DONE;
  }
  requestUpdate();
}

void BookOrbitCatalogActivity::openDownloadedBook() {
  if (state != State::DONE || selectedBookRow < 0) return;
  const auto& book = bookAt(selectedBookRow);
  const auto& path = book.downloadedPath;
  if (book.downloadedRevision == Storage.resourceRevision() && isPresentEpub(path)) {
    app.clearTapFlash();
    onSelectBook(path);
  } else {
    activateIndex(selectedBookRow);
  }
}

void BookOrbitCatalogActivity::downloadFinished(bool) {
  state = State::BROWSING;
  requestUpdate();
}

void BookOrbitCatalogActivity::onBackButton() {
  if (state != State::DONE) {
    CatalogActivity::onBackButton();
    return;
  }
  state = State::BROWSING;
  requestUpdate();
}

bool BookOrbitCatalogActivity::handleButtons() {
  if (state == State::DONE) {
    if (mappedInput.wasReleased(MappedInputManager::Button::Back))
      onBackButton();
    else if (mappedInput.wasReleased(MappedInputManager::Button::Confirm))
      openDownloadedBook();
    else if (routeTouch(mappedInput).routed && app.invalidated())
      requestUpdate();
    return true;
  }
  return UiListActivity::handleButtons();
}

void BookOrbitCatalogActivity::drawFooter() {
  if (mappedInput.hasTouch()) return;
  const char* confirm = state == State::BROWSING ? tr(STR_FETCH)
                        : state == State::DONE   ? tr(STR_OPEN)
                        : state == State::ERROR  ? tr(STR_RETRY)
                                                 : "";
  const char* up = state == State::BROWSING ? (nav.selected == 0 ? tr(STR_SEARCH) : tr(STR_DIR_UP)) : "";
  const char* down = state == State::BROWSING ? tr(STR_DIR_DOWN) : "";
  const auto labels =
      mappedInput.mapLabels(state == State::DOWNLOADING ? tr(STR_CANCEL) : tr(STR_BACK), confirm, up, down);
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}

void BookOrbitCatalogActivity::buildScreen(UiScreen& screen) {
  pixelui::FontScope fonts(uiTarget, renderer, I18N.getLanguage() != Language::EN);
  const bool search = state == State::BROWSING;
  catalogui::header(screen, tr(STR_BOOKORBIT), toybox::headerStatus(renderer),
                    search ? fui::bitmapFromIcon(icon_search_32) : fui::BitmapRef{},
                    search ? ACTION_SEARCH : fui::NO_ACTION, !chrome::lightHeader);
  const auto& metrics = UITheme::getInstance().getMetrics();
  libraryui::beginFileRows(screen, mappedInput.hasTouch() ? 0 : metrics.buttonHintsHeight);
  if (buildStatusScreen(screen, false)) return;
  if (state == State::DONE) {
    fui::ButtonProps open;
    open.label = tr(STR_BOOKORBIT_OPEN_BOOK);
    open.action = ACTION_OPEN_BOOK;
    open.inputMask = fui::InputTouch;
    open.text.font = pixelui::kSmall;
    open.text.align = fui::TextAlign::Center;
    open.styles = pixelui::buttonStyles();
    open.styles.normal = open.styles.selected;
    open.radius = lightui::radius();
    screen.button(open, screen.takeBottom(58, 10));
    catalogCenteredBlock(screen, {{tr(STR_BOOKORBIT_DOWNLOADED), true}, {bookAt(selectedBookRow).title.c_str()}});
    return;
  }
  if (rowCount == 0) {
    screen.centeredText(tr(STR_NO_ENTRIES), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.items = rows.data();
  props.count = static_cast<uint16_t>(rowCount);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  uiTarget.setFont(4, toybox::kBookListFontId);
  libraryui::styleBookRows(props, screen.target());
  if (chrome::lightHeader) libraryui::lightRows(props);
  libraryui::fitPortraitRows(props, screen.device(), screen.body());
  props.rtl =
      I18N.getLanguage() == Language::AR || I18N.getLanguage() == Language::FA || I18N.getLanguage() == Language::HE;
  syncListViewport(screen, props);
  props.partialTrailingRow = false;
  if (chrome::lightHeader)
    fui::list(screen.frame(), screen.body(), props);
  else
    pixelui::list(screen, props);
}

}  // namespace bookorbit
