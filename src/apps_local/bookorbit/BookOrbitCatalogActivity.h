#pragma once

#include <ArduinoJson.h>
#include <KOReaderSyncClient.h>

#include <array>

#include "activities/CatalogActivity.h"

namespace bookorbit {

class BookOrbitCatalogActivity final : public CatalogActivity {
 public:
  BookOrbitCatalogActivity(GfxRenderer& renderer, MappedInputManager& input, KOReaderEndpoint account,
                           std::string folder);
  void onEnter() override;
  void onExit() override;

 private:
  static constexpr int PAGE_SIZE = 12;
  static constexpr freeink::ui::ActionId ACTION_OPEN_BOOK = ACTION_USER + 2;
  struct Book {
    int64_t id = 0;
    std::string title;
    std::string author;
    std::string downloadedPath;
    uint32_t downloadedRevision = 0;
  };

  std::string apiUrl, folder, query;
  std::string serverInstance;
  bool checkedServerIdentity = false;
  std::vector<HttpDownloader::Header> headers;
  std::array<Book, PAGE_SIZE> books;
  std::array<freeink::ui::ListItem, PAGE_SIZE + 2> rows{};
  int page = 1;
  int rowCount = 0;
  int selectedBookRow = -1;
  bool hasNextPage = false;
  bool startedWifi = false;

  int listCount() const override { return state == State::BROWSING ? rowCount : 0; }
  bool hasSearch() const override { return true; }
  std::string searchPrefill() const override { return query; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  bool handleButtons() override;
  void onBackButton() override;
  void drawFooter() override;
  void startBrowse() override;
  void performSearch(const std::string& text) override;
  void downloadFinished(bool cancelled) override;
  bool fetchJson(const std::string& url, const JsonDocument& filter, JsonDocument& result);
  Book& bookAt(int row) { return books[row - (page > 1 ? 1 : 0)]; }
  void downloadBook(Book& book);
  void showDownloaded(std::string path);
  void openDownloadedBook();
};

}  // namespace bookorbit
