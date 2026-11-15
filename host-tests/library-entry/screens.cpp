#include "components/UITheme.h"
#include "src/apps_local/library/LibraryEntryBackdrop.h"
#include "src/components/icons/cover.h"
#define main originalLibraryChecks
#include "../library-first-frame/screens.cpp"
#undef main
#include "default-placeholder.inc"

void entryScreens(const char* output) {
  Font small(toybox_10), body(toybox_14), title(toybox_20), prose(ui_prose_14);
  for (bool wide : {false, true})
    for (bool light : {false, true})
      for (bool covers : {false, true})
        for (int tab : {0, 1})
          for (bool english : {false, true})
            for (int count : {-1, 0, 1, 3, 9, 14, 30})
              for (int selected : {0, 29}) {
                int width = wide ? 800 : 480, height = wide ? 480 : 800;
                std::vector<uint8_t> bits(width * height / 8, 255);
                Target target(bits.data(), width, height, width / 8);
                target.setFont(0, small.font);
                target.setFont(1, body.font);
                target.setFont(2, title.font);
                target.setFont(3, small.font);
                target.setFont(4, prose.font);
                auto device = target.deviceContext();
                device.hasTouch = true;
                device.safeArea = {10, 1, 0, 1};
                toybox::Interactions hits;
                fui::InputSnapshot input{};
                toybox::Frame frame(target, device, input, hits);
                toybox::Screen screen(frame);
                I18N.setLanguage(english ? Language::EN : Language::ES);
                chrome::lightHeader = light;
                libraryui::HeaderWork header;
                libraryui::header(screen, header, tr(STR_LIBRARY), false, {}, {}, 0, 0);
                pixelui::begin(screen, covers ? 0 : libraryui::kFooterHeight);
                libraryui::filterTabs(screen, tab, fui::NO_ACTION);
                const auto bodyArea = screen.body();
                GfxRenderer renderer(target);
                const size_t before = target.runs.size();
                const auto outlineCount = target.outlines.size();
                auto noticeArea = libraryui::entryBackdrop(renderer, target, device, bodyArea, covers, count, selected);
                const auto grid = libraryui::gridLayout(device, bodyArea);
                const int first = std::clamp(selected, 0, std::max(0, count - 1)) / grid.capacity() * grid.capacity();
                const int visible = covers ? std::clamp(count - first, 0, grid.capacity()) : 0;
                CHECK(renderer.placeholders == visible);
                const int pagerOutlines = covers && count >= 0 ? (light ? 1 : 3) : 0;
                CHECK(target.outlines.size() - outlineCount == unsigned(visible + pagerOutlines));
                for (int n = 0; n < visible; ++n) {
                  const auto box = grid.card(n);
                  const auto actual = target.outlines[outlineCount + n];
                  CHECK(actual.x == box.x && actual.y == box.y && actual.width == box.width &&
                        actual.height == box.height);
                  const int x = box.x + box.width / 2;
                  const auto pixel = [&](int y) { return (bits[y * (width / 8) + x / 8] >> (7 - x % 8)) & 1; };
                  CHECK(pixel(box.y) == 0 && pixel(box.y + 1) == 1);
                  CHECK(pixel(box.y + box.height / 2) == 0);
                }
                CHECK(target.runs.size() == before + (covers && count >= 0 ? 1 : 0));
                if (covers && count >= 0) {
                  char expected[32];
                  std::snprintf(expected, sizeof(expected), "%d / %d", first / grid.capacity() + 1,
                                std::max(1, (count + grid.capacity() - 1) / grid.capacity()));
                  CHECK(target.runs.back().text == expected);
                  auto entryBits = bits;
                  pageui::draw(target, grid.pager, first / grid.capacity() + 1,
                               (count + grid.capacity() - 1) / grid.capacity());
                  CHECK(entryBits == bits);
                }
                CHECK(noticeArea.y >= bodyArea.y && noticeArea.bottom() <= bodyArea.bottom());
                if (covers) {
                  libraryui::loadingNotice(target, noticeArea, tr(STR_LIBRARY_LOADING));
                  CHECK(target.runs.back().text == tr(STR_LIBRARY_LOADING));
                } else
                  CHECK(target.runs.size() == before);
                CHECK(!hits.overflowed());
                char filename[1024];
                std::snprintf(filename, sizeof filename, "%s/%s-%s-%s-tab%d-%s-count%d-selected%d.pbm", output,
                              wide ? "wide" : "portrait", light ? "light" : "boxed", covers ? "covers" : "list", tab,
                              english ? "en" : "es", count, selected);
                auto* file = std::fopen(filename, "wb");
                CHECK(file);
                std::fprintf(file, "P4\n%d %d\n", width, height);
                for (auto byte : bits) std::fputc(byte ^ 255, file);
                std::fclose(file);
              }
}
int main(int argc, char** argv) {
  CHECK(argc == 2);
  originalLibraryChecks(argc, argv);
  entryScreens(argv[1]);
  contract::LibraryListActivity a;
  a.entryPhase = contract::LibraryListActivity::EntryPhase::Shell;
  CHECK(a.entryBookCount() == 9);
  contract::SETTINGS.libraryFileTypes = 2;
  CHECK(a.entryBookCount() == 0);
  contract::RECENT_BOOKS.books[0].path = "/books/test.xtch";
  CHECK(a.entryBookCount() == 1);
  contract::SETTINGS.libraryDisplay = 0;
  CHECK(a.entryBookCount() == -1);
  contract::SETTINGS.libraryDisplay = libraryui::kFinished;
  a.facetFilter.enabled = true;
  CHECK(a.entryBookCount() == -1);
  a.facetFilter.enabled = false;
  a.query = "test";
  CHECK(a.entryBookCount() == -1);
  a.entryPhase = contract::LibraryListActivity::EntryPhase::Ready;
  CHECK(a.entryBookCount() == a.listCount());
  std::printf("Library entry: %u checks passed\n", checks);
}
