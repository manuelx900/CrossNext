#include <GfxRenderer.h>

#include "src/apps_local/readerbars/ReaderBars.h"
using namespace readerbars;
bool validUtf8(const char* text) {
  while (*text) {
    const auto byte = static_cast<unsigned char>(*text++);
    int rest = byte < 0x80 ? 0 : byte < 0xe0 ? 1 : byte < 0xf0 ? 2 : 3;
    while (rest--)
      if ((static_cast<unsigned char>(*text++) & 0xc0) != 0x80) return false;
  }
  return true;
}
int main(int argc, char** argv) {
  for (int orientation = 0; orientation < 2; ++orientation) {
    GfxRenderer r(orientation ? 800 : 480, orientation ? 480 : 800);
    Workspace workspace;
    Config top, bottom;
    Data data;
    data.book = "Alice's Adventures in Wonderland";
    data.chapter = "CHAPTER VII - A Mad Tea-Party";
    data.percent = 42;
    data.page = 8;
    data.pages = 32;
    data.timeBook = 5400;
    data.timeChapter = 720;
    top.slots = {Item::Clock, Item::Empty, Item::Empty, Item::Book, Item::Empty, Item::Empty, Item::Battery};
    bottom.slots = {Item::TimeChapter, Item::Empty,   Item::Empty, Item::Chapter,
                    Item::Pages,       Item::Percent, Item::Empty};
    bottom.progress = Progress::Book;
    draw(r, top, Position::Top, data, workspace);
    draw(r, bottom, Position::Bottom, data, workspace);
    for (int y = 72; y < r.height - 70; y += 35) r.drawText(0, 24, y, "There was a table under a tree.");
    if (argc > 1) {
      char path[160];
      std::snprintf(path, sizeof(path), "%s-%d.pbm", argv[1], r.width);
      r.write(path);
    }
    top.slots.fill(Item::Book);
    bottom.slots.fill(Item::Chapter);
    data.book = "Very long title with repeated words to verify that seven populated slots never overlap";
    draw(r, top, Position::Top, data, workspace);
    draw(r, bottom, Position::Bottom, data, workspace);
    data.book = "Título larguísimo: árboles, pingüinos y páginas de una edición española muy extensa";
    draw(r, top, Position::Top, data, workspace);
    for (const auto& field : workspace.text) assert(validUtf8(field));
    char shortened[64];
    copyLabel(shortened, data.book);
    assert(validUtf8(shortened));
    assert(std::strlen(shortened) <= 63);
    assert(std::strcmp(shortened + std::strlen(shortened) - 3, "...") == 0);
    top.visible = false;
    const int before = r.measures;
    draw(r, top, Position::Top, data, workspace);
    assert(r.measures == before);
    std::printf("production renderer %dx%d: bounds checked, hidden bar does no measuring\n", r.width, r.height);
  }
}
