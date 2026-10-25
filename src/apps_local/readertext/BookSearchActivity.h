#pragma once
#include <Epub.h>
#include <I18n.h>
#include <Print.h>
#include <ZipEntryReader.h>
#include <expat.h>

#include "../../activities/UiListActivity.h"
class BookSearchActivity final : public UiListActivity {
 public:
  BookSearchActivity(GfxRenderer& renderer, MappedInputManager& input, std::shared_ptr<Epub> book)
      : UiListActivity("BookSearch", renderer, input), book(std::move(book)) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;

 protected:
  int listCount() const override { return count; }
  const char* headerTitle() const override { return tr(STR_READER_SEARCH); }
  void drawChrome() override;
  void drawFooter() override {}
  void buildScreen(UiScreen&) override;
  void activateIndex(int) override;

 private:
  struct Match {
    uint16_t spine = 0;
    char anchor[96]{}, snippet[128]{};
  };
  // 64 bounded chapter results (~14 KiB), owned by the fallible activity allocation.
  // It must outlive keyboard/list frames and cannot live on the task stack.
  Match matches[64]{};
  struct SearchStream final : Print {
    XML_Parser parser = nullptr;
    char query[65]{}, prefix[65]{}, recent[128]{}, snippet[128]{}, anchor[96]{}, matchAnchor[96]{};
    int length = 0, matched = 0, used = 0, recentEnd = 0, recentCount = 0, skip = 0, after = 0;
    bool found = false, failed = false, previousSpace = true;
    unsigned char previousByte = 0;
    ~SearchStream();
    bool begin(const char*);
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t*, size_t) override;
    static void XMLCALL text(void*, const XML_Char*, int);
    static void XMLCALL start(void*, const XML_Char*, const XML_Char**);
    static void XMLCALL end(void*, const XML_Char*);
  } stream;
  std::shared_ptr<Epub> book;
  char query[65]{}, status[128]{};
  freeink::ui::ListProps props;
  freeink::ui::ListItem row;
  int count = 0, spine = 0, errors = 0;
  bool searching = false, plainText = false;
  // One KiB output and one KiB ZIP input persist between loop turns.
  uint8_t chunk[1024]{};
  ZipEntryReader input;
  HalFile textFile;
  void completeChapter(bool ok);
  void closeInput();
  freeink::ui::ButtonProps searchButton;
  void openQuery();
  static void provide(void*, uint16_t, freeink::ui::ListItem&);
};
