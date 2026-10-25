#include "NotesScreens.h"

#include <FreeInkUIIcon.h>
#include <I18n.h>

#include <cstdio>
#include <string>
#include <vector>

#include "../ui/CompactArt.h"
#include "../ui/Dashboard.h"
#include "../ui/FitText.h"
#include "../ui/PageIndicator.h"
#include "../ui/ReadingToolsUi.h"
#include "../ui/ToyboxIcons.h"
#include "../ui/ToyboxText.h"

namespace notesui {
namespace {

constexpr int kBodyTop = toybox::kBodyTop;
constexpr int kFooterHeight = toybox::kPillHeight;
constexpr int kBoxSide = 40;
constexpr int16_t kStrike = 2;  // a done line's bar: one crisp rule, not a grey dither
// One height for every row of a sheet, so the menu, the confirm and the notice
// agree about where a row starts.

constexpr int kMinRow = 72;  // a finger, with room to miss

int16_t pageWidth(const fui::DeviceContext& device) { return static_cast<int16_t>(device.width - 2 * toybox::kMargin); }

fui::TextStyle plain(const fui::FontId font, const fui::TextAlign align = fui::TextAlign::Left,
                     const uint8_t maxLines = 1) {
  fui::TextStyle style;
  style.font = font;
  style.align = align;
  style.color = fui::Color::Black;
  style.maxLines = maxLines;
  return style;
}

// Header band, rule, page margin. The title is fitted before it goes on the
// band: the header component elides, kHeaderHeight is load-bearing so the band
// cannot grow, and the Toybox cuts above toybox_10 carry no ellipsis glyph at
// all -- an overflow there draws as a name that simply stops.
void chrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr,
            const freeink::Icon* trailing = nullptr, const fui::FontId nameCut = toybox::kUiFont,
            int rightReserve = 0) {
  (void)nameCut;
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.back = screen.device().hasTouch;
  header.rightLabel = rightLabel;
  header.titleReserve = rightReserve;
  if (trailing) header.mark = fui::bitmapFromIcon(*trailing);
  pixelui::header(screen, header);
  const auto g = pixelui::geometry(screen.device());
  backui::publish(header.back, g.back);
  if (trailing) {
    const auto band =
        fui::makeRect(g.back.right() + 4, g.header.y, g.header.right() - g.back.right() - 4, g.header.height);
    screen.frame().hit(band, ActionMenu, 0, fui::InputTouch);
  }
}

// A row's tap target, invisible: the whole row, because a 40px box is a miss
// waiting to happen and a miss costs two refreshes, the wrong one and the undo.
void rowHit(toybox::Screen& screen, const fui::Rect& row, const fui::ActionId action, const int index) {
  fui::ButtonProps hit;
  hit.label = "";
  hit.action = action;
  hit.value = static_cast<int16_t>(index);
  hit.styles = toybox::rowStyles();
  hit.styles.normal.background = fui::Paint::none();
  hit.styles.normal.border = fui::Paint::none();
  screen.button(hit, row);
}

// Never on the last row of a page: a rule against the band edge, or against the
// bar under it, reads as a double line and is the one place a list looks
// unfinished rather than divided.

// One line that must not overflow its box, set at the largest cut that holds it.
// Toybox's rule is that nothing is elided, and the cuts above toybox_10 carry no
// ellipsis glyph at all -- an overflow there draws as a sentence that stops at a
// plausible place, and the screenshot looks fine.
void fittedLine(toybox::Screen& screen, const fui::Rect& box, const char* text, const fui::TextAlign align,
                const fui::FontId font) {
  fui::TextStyle style = plain(font, align);
  const std::string drawn = toybox::fittedTitle(screen.target(), text, box.width, style);
  screen.target().text(box, drawn.c_str(), style);
}

// The tick box: an outline, filled with a smaller solid square when done. Pure
// black on pure white in the one small rect that changes, which is the fastest
// and least ghost-prone update this panel can perform. No tick glyph -- the
// Toybox face is ASCII and a check mark is not in it.
void tickBox(toybox::Screen& screen, const fui::Rect& box, const bool checked) {
  screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), toybox::kRule, 4);
  if (!checked) return;
  const int16_t inset = 9;
  screen.target().fill(
      fui::makeRect(static_cast<int16_t>(box.x + inset), static_cast<int16_t>(box.y + inset),
                    static_cast<int16_t>(box.width - 2 * inset), static_cast<int16_t>(box.height - 2 * inset)),
      fui::Paint::solid(fui::Color::Black), 2);
}

// The strike is measured against the LONGEST DRAWN LINE, not the source string,
// so a line that wrapped gets a rule the width of what is really on the glass.
// THE ONE LINE-BREAK RULE IN THIS FILE. A row is sized from it, drawn from it
// one line at a time, and struck through from it, so the three cannot disagree.
//
// They used to be three rules. The row was sized by adding word
// widths; the renderer wrapped the text again by its own algorithm when handed
// a multi-line run; and the strike split the string on '\n', which wrapped text
// does not contain -- so a ticked item that ran to two lines was struck on the
// first only, with a bar as wide as the whole sentence. Nothing here hands the
// renderer a multi-line run any more: every line it draws is a line this
// function chose.
//
// Measured as whole candidate lines, never as a sum of words, because a sum is
// not what the panel draws: spacing and pairs are only right when the run is
// measured as the run.
struct Broken {
  std::vector<std::string> lines;
  bool fits = true;  // false: a word wider than `width`, or more than maxLines
};

Broken breakWords(const fui::DrawTarget& target, const std::string& text, const int16_t width, const int maxLines,
                  const fui::TextStyle& style, const bool splitLongWords) {
  Broken out;
  if (width <= 0 || maxLines <= 0) {
    out.fits = false;
    return out;
  }
  // Nothing this long can fit a 480px row, and the byte cap is what keeps the
  // measure honest: widths come back as int16_t, so a run wider than 32767px
  // wraps NEGATIVE, reads as fitting, and is then silently cut by the
  // renderer -- a 4000-byte word used to vanish that way. A glyph narrower
  // than three pixels would be needed to put this many bytes on one row.
  constexpr size_t kLongestLineBytes = 160;
  const size_t charsPerLine = std::max(1, int(width) / std::max(1, int(target.lineHeight(style.font))));
  out.lines.reserve(std::min(text.size() + 1, text.size() / charsPerLine + 2));
  const auto fits = [&target, &style, width](const std::string& run) {
    if (run.size() > kLongestLineBytes) return false;
    const int16_t measured = target.measureText(style.font, run.c_str(), style).width;
    return measured >= 0 && measured <= width;
  };
  std::string line;
  size_t i = 0;
  while (i <= text.size()) {
    size_t end = text.find(' ', i);
    if (end == std::string::npos) end = text.size();
    std::string word = text.substr(i, end - i);
    i = end + 1;
    if (!word.empty()) {
      const std::string candidate = line.empty() ? word : line + " " + word;
      if (fits(candidate)) {
        line = candidate;
      } else {
        if (!line.empty()) out.lines.push_back(line);
        line.clear();
        if (!fits(word)) {
          if (!splitLongWords) out.fits = false;
          // Only reachable at the last-resort cut, with one run of letters
          // wider than the row. Cut at a CHARACTER boundary, never inside a
          // UTF-8 sequence, which would draw as a box.
          //
          // BINARY SEARCH over the character boundaries, not a walk down from
          // the end: that measured every prefix of every remaining tail, which
          // is cubic in bytes -- 53K measures and 37MB scanned for one pasted
          // 2000-byte run, on every relayout.
          while (!word.empty() && !fits(word)) {
            std::vector<size_t> ends;  // byte offsets that end a whole character
            ends.reserve(word.size());
            for (size_t at = 1; at <= word.size(); at++) {
              if (at == word.size() || (static_cast<unsigned char>(word[at]) & 0xC0) != 0x80) ends.push_back(at);
            }
            // The longest prefix that fits. Always at least one whole character,
            // even one wider than the row on its own: a cut that takes nothing
            // never ends.
            size_t lo = 0;
            size_t hi = ends.size() - 1;
            while (lo < hi) {
              const size_t mid = (lo + hi + 1) / 2;
              if (fits(word.substr(0, ends[mid]))) {
                lo = mid;
              } else {
                hi = mid - 1;
              }
            }
            const size_t n = ends[lo];
            out.lines.push_back(word.substr(0, n));
            word = word.substr(n);
          }
        }
        line = word;
      }
    }
    if (end >= text.size()) break;
  }
  if (!line.empty()) out.lines.push_back(line);
  if (out.lines.empty()) out.lines.push_back(std::string());
  if (static_cast<int>(out.lines.size()) > maxLines) {
    out.fits = false;
    out.lines.resize(static_cast<size_t>(maxLines));
  }
  return out;
}

// Each line its own single-line run, at its own height, and -- when `struck` --
// its own bar, exactly as wide as that line's ink. A strike is part of the line
// it crosses, so it is drawn by the same loop that draws the line.
void drawLines(toybox::Screen& screen, const fui::Rect& box, const std::vector<std::string>& lines,
               const fui::TextStyle& style, const bool struck, const int16_t advance = 0) {
  const int16_t lineHeight = screen.target().lineHeight(style.font);
  const int16_t step = advance > 0 ? advance : lineHeight;
  fui::TextStyle one = style;
  one.maxLines = 1;
  for (size_t i = 0; i < lines.size(); i++) {
    const int16_t top = static_cast<int16_t>(box.y + static_cast<int>(i) * step);
    screen.target().text(fui::makeRect(box.x, top, box.width, lineHeight), lines[i].c_str(), one);
    if (!struck || lines[i].empty()) continue;
    const int16_t width = screen.target().measureText(one.font, lines[i].c_str(), one).width;
    screen.target().fill(fui::makeRect(box.x, static_cast<int16_t>(top + lineHeight / 2), width, kStrike),
                         fui::Paint::solid(fui::Color::Black));
  }
}

// Row height comes from the TYPE, never from how many rows there are. Dividing
// the band by the count fills a short page, but it also means the same note is
// drawn with different spacing after one line is added, and a list whose rhythm
// changes as you use it reads worse than one that ends early. So: the lines the
// When everything fits on one page, the rows SHARE the band instead of stacking
// at the top under a hole. Capped, so a two-item list is not two slabs, and
// only while nothing is paged, so a row never changes size under the finger
// because a note grew past the fold.

// The bar every screen's actions live on: one y, one height, on the deck, the
// note and the sheets alike, so the thumb learns a single place. The filled
// button is always the one that makes something, and the outlined one on the
// RIGHT is always the one that takes something away -- CLEAR DONE, DELETE.
fui::Rect footerBand(const fui::DeviceContext& device) {
  return fui::makeRect(toybox::kMargin, static_cast<int16_t>(device.height - toybox::kMargin - kFooterHeight),
                       static_cast<int16_t>(device.width - 2 * toybox::kMargin), kFooterHeight);
}

// The page between the chrome and that bar.
fui::Rect sheetBand(const fui::DeviceContext& device) {
  const fui::Rect footer = footerBand(device);
  return fui::makeRect(toybox::kMargin, kBodyTop, footer.width,
                       static_cast<int16_t>(footer.y - toybox::kGutter * 2 - kBodyTop));
}

void footerButton(toybox::Screen& screen, const fui::Rect& box, const char* label, const fui::ActionId action,
                  const bool outlined) {
  dashboard::button(screen, box, label, action, 0, !outlined, nullptr, 1);
}

// Prose set from the top of a sheet's page, at the body cut, wrapped to as many
// lines as the page holds.
void sheetProse(toybox::Screen& screen, const fui::Rect& band, const char* text) {
  fui::TextStyle prose = plain(4, fui::TextAlign::Left, 5);
  const int16_t lineHeight = screen.target().lineHeight(prose.font);
  const int maxLines = band.height / lineHeight;
  prose.maxLines = static_cast<uint8_t>(maxLines < 1 ? 1 : maxLines);
  const std::string drawn = toybox::fitLines(screen.target(), text, band.width, prose.maxLines, prose);
  screen.target().text(fui::makeRect(band.x, band.y, band.width, band.height), drawn.c_str(), prose);
}

// "1 / 2" in the strip under the band. It is small, centred and the only thing
// down there, so it reads as a page number rather than as a control.
void pageLabel(toybox::Screen& screen, const fui::Rect& band, const char* label) {
  if (!label) return;
  int page = 1, total = 1;
  if (std::sscanf(label, "%d / %d", &page, &total) != 2) return;
  pageui::build(screen, fui::makeRect(band.x, band.bottom() - 44, band.width, 44), page, total, ActionPage, -1,
                ActionPage, 1);
}

void centredNotice(toybox::Screen& screen, const fui::Rect& band, const char* text) {
  fui::TextStyle style = plain(4, fui::TextAlign::Center, 3);
  const int16_t lineHeight = screen.target().lineHeight(style.font);
  const fui::Rect box = fui::makeRect(band.x, static_cast<int16_t>(band.y + (band.height - lineHeight * 3) / 2),
                                      band.width, static_cast<int16_t>(lineHeight * 3));
  const std::string drawn = toybox::fitLines(screen.target(), text, box.width, 3, style);
  screen.target().text(box, drawn.c_str(), style);
}

}  // namespace

// --- The deck ------------------------------------------------------------

namespace {

struct DeckLayout {
  int visible = 5;
};
DeckLayout deckLayoutFor(const fui::Rect& band) { return {band.width > band.height ? 2 : 5}; }
void deckRows(toybox::Screen& screen, const DeckModel& model, const fui::Rect& band, const DeckLayout& layout,
              int16_t& y) {
  const int h = (band.height - 8 * (layout.visible - 1)) / layout.visible;
  for (int n = 0; n < layout.visible && model.firstVisible + n < model.count; ++n) {
    const int index = model.firstVisible + n;
    const auto& item = model.items[index];
    const auto r = fui::makeRect(band.x, band.y + n * (h + 8), band.width, h);
    dashboard::frame(screen, r, 2);
    readingui::note(screen, fui::makeRect(r.x + 14, r.y + (r.height - 56) / 2, 50, 56));
    readingui::text(screen, fui::makeRect(r.x + 82, r.y + 7, r.width - 126 - (item.asleep ? 40 : 0), 44), item.title,
                    readingui::kReadingFont, 1);
    readingui::text(screen, fui::makeRect(r.x + 82, r.bottom() - 36, r.width - 126 - (item.asleep ? 40 : 0), 30),
                    item.preview ? item.preview : "", 4, 1);
    if (item.asleep)
      compactart::draw(screen, fui::makeRect(r.right() - 76, r.y + (r.height - 32) / 2, 32, 32), compactart::ListMoon);
    dashboard::chevron(screen.target(), fui::makeRect(r.right() - 34, r.y, 28, r.height));
    dashboard::hit(screen, r, ActionOpenNote, index);
    y = r.bottom() + 8;
  }
}

}  // namespace

namespace {
// The band both deck functions measure against. One definition, so capacity and
// drawing cannot drift apart.
fui::Rect deckBand(const fui::DeviceContext& device) {
  const auto b = pixelui::geometry(device).body.inset(fui::Insets{4, 4, 0, 4});
  return fui::makeRect(b.x, b.y + 92, b.width, b.height - 218);
}
}  // namespace

namespace {
fui::Rect todoBody(const fui::DeviceContext& device) {
  return pixelui::geometry(device, 12).body.inset(fui::Insets{8, 4, 0, 4});
}
void todoChrome(toybox::Screen& screen, const char* title, bool settings = false,
                fui::ActionId route = ActionTodoLists) {
  pixelui::begin(screen);
  pixelui::Header header;
  header.title = title;
  header.back = screen.device().hasTouch;
  header.titleReserve = settings ? 112 : 56;
  pixelui::header(screen.target(), screen.device(), header);
  const auto g = pixelui::geometry(screen.device());
  backui::publish(header.back, g.back);
  for (int n = 0; n < (settings ? 2 : 1); ++n) {
    const auto rect = fui::makeRect(screen.device().safeRect().right() - 60 - n * 56, g.header.bottom() - 60, 48, 48);
    screen.target().stroke(rect, dashboard::paper, 4);
    dashboard::icon(screen, rect.inset(fui::Insets{7, 7, 7, 7}),
                    settings && n == 0 ? icon_go_settings_32 : extra_list_32, true);
    dashboard::hit(screen, rect, settings && n == 0 ? ActionMenu : route);
  }
}
void todoPager(toybox::Screen& screen, fui::Rect band, int page, int total) {
  pageui::build(screen, fui::makeRect(band.x, band.bottom() - 44, band.width, 44), page, total, ActionPage, -1,
                ActionPage, 1);
}
}  // namespace
int deckCapacity(const fui::DeviceContext& device, bool todo) {
  const fui::Rect band = deckBand(device);
  return todo ? (device.width > device.height ? 3 : 6) : deckLayoutFor(band).visible;
}

void buildDeck(toybox::Screen& screen, const DeckModel& model) {
  const auto device = screen.device();
  if (model.todo) {
    todoChrome(screen, tr(STR_TODO_MY_LISTS), false, ActionFreeTasks);
    const auto band = todoBody(device);
    const int capacity = deckCapacity(device, true), rowH = (band.height - 136) / capacity;
    for (int n = 0; n < capacity && model.firstVisible + n < model.count; ++n) {
      const int index = model.firstVisible + n;
      const auto& item = model.items[index];
      const auto row = fui::makeRect(band.x, band.y + n * rowH, band.width, rowH);
      dashboard::frame(screen, row, 2);
      dashboard::icon(screen, fui::makeRect(row.x + 18, row.y + (row.height - 40) / 2, 40, 40), extra_list_32);
      dashboard::text(screen, fui::makeRect(row.x + 80, row.y, row.width - 204, row.height), item.title, 1,
                      fui::TextAlign::Left);
      char label[32];
      std::snprintf(label, sizeof(label), "%d / %d", item.done, item.total);
      dashboard::text(screen, fui::makeRect(row.right() - 126, row.y, 90, row.height), label, 0);
      dashboard::chevron(screen.target(), fui::makeRect(row.right() - 36, row.y, 26, row.height));
      dashboard::hit(screen, row, ActionOpenNote, index);
    }
    if (!model.count) {
      const auto notice = fui::makeRect(band.x + 12, band.y + 24, band.width - 24, band.height - 160);
      const int lines = std::max(1, int(notice.height) / std::max(1, int(screen.target().lineHeight(4))));
      readingui::text(screen, notice, tr(STR_TODO_EMPTY), 4, lines, fui::TextAlign::Center);
    }
    todoPager(screen, fui::makeRect(band.x, band.bottom() - 124, band.width, 48), model.firstVisible / capacity + 1,
              std::max(1, (model.count + capacity - 1) / capacity));
    dashboard::action(screen, fui::makeRect(band.x, band.bottom() - 64, band.width, 64), tr(STR_TASK_NEW_LIST),
                      ActionNewList, &extra_dashboard_add_32, true);
    return;
  }
  const auto b = readingui::begin(screen, tr(STR_NOTES_APP));
  char count[40];
  std::snprintf(count, sizeof(count), model.count == 1 ? tr(STR_RT_NOTE_TALLY_ONE) : tr(STR_RT_NOTES_TALLY),
                model.count);
  readingui::caps(screen, fui::makeRect(b.x, b.y, b.width, 32), count, 0);
  const auto sort = fui::makeRect(b.x, b.y + 38, b.width, 44);
  dashboard::frame(screen, sort, 2);
  readingui::caps(screen, fui::makeRect(sort.x + 10, sort.y, sort.width - 64, sort.height),
                  model.recent ? tr(STR_RT_RECENT) : tr(STR_RT_ALPHABETICAL), 0);
  readingui::sort(screen, fui::makeRect(sort.right() - 48, sort.y, 44, 44));
  dashboard::hit(screen, sort, ActionSort);
  const auto band = deckBand(device);
  const auto layout = deckLayoutFor(band);
  int16_t y = band.y;
  deckRows(screen, model, band, layout, y);
  if (!model.count) centredNotice(screen, band, tr(STR_NOTES_EMPTY));
  todoPager(screen, fui::makeRect(b.x, b.bottom() - 116, b.width, 44), model.firstVisible / layout.visible + 1,
            std::max(1, (model.count + layout.visible - 1) / layout.visible));
  dashboard::button(screen, fui::makeRect(b.x, b.bottom() - 64, b.width, 64), tr(STR_RT_NEW_NOTE), ActionNewPage, 0,
                    true, &extra_dashboard_add_32, 1);
}

// --- A note, open --------------------------------------------------------

namespace {

// --- The note: every row the body cut, every row as tall as its own words ---
//
// ONE CUT FOR EVERY ROW, AND IT IS THE BODY CUT. Each row is as tall as its own
// text. The first two versions chose one geometry for the whole list from its
// LONGEST item: every row was sized for the tallest, and when any item would
// not fit two body lines the whole list dropped to toybox_10. One pasted
// sentence turned a shopping list into small print with three-line gaps, a
// four-item list paged because every row was the height of its longest, and a
// note with one long paragraph was set entirely in the smallest face. Peers
// still share a cut. They no longer share a height.

constexpr int16_t kItemPad = 16;  // above and below an item's text block
constexpr int16_t kParaGap = 12;  // between two paragraphs of a note
constexpr int16_t kGrowCap = 36;  // the most a row grows when the whole list fits one page

// A list: one block per item. A note: one block per LINE, so a long paragraph
// continues on the next page rather than being shrunk or cut.
struct Block {
  int item = 0;
  std::vector<std::string> lines;  // exactly what is drawn, already broken
  int16_t height = 0;              // before any growth
  bool section = false;
  bool paragraphStart = false;  // a note: the first line of a paragraph
};

struct NoteFlow {
  bool prose = false;  // a note's lines rather than a list's items
  bool ruled = false;
  int paragraphGap = kParaGap;
  fui::TextStyle body{};
  int16_t lineHeight = 0;
  int16_t textWidth = 0;
  std::vector<Block> blocks;
};

// Room the page label takes at the foot of a paged band.
int16_t pageLabelReserve(const fui::DrawTarget& target) {
  return static_cast<int16_t>(std::max(44, int(target.lineHeight(toybox::kTileFont))) + toybox::kGutter);
}

NoteFlow flowNote(const fui::DrawTarget& target, const NoteModel& model, const fui::Rect& band) {
  NoteFlow flow;
  flow.prose = model.page;
  const bool dense = model.todo && !model.asleep;
  flow.ruled = model.page && !model.todo && !model.asleep;
  flow.paragraphGap = flow.ruled ? 48 : kParaGap;
  flow.body = plain(flow.ruled ? readingui::kReadingFont : 4);
  flow.lineHeight = target.lineHeight(flow.body.font);
  if (flow.lineHeight <= 0) flow.lineHeight = 1;
  flow.textWidth =
      model.page ? band.width : static_cast<int16_t>(band.width - kBoxSide - toybox::kGutter - (model.todo ? 24 : 0));

  // Long tasks continue on another page, with every segment bound to the same item.
  const int usable = band.height - (dense ? 16 : pageLabelReserve(target) + 2 * kItemPad + (model.todo ? 56 : 0));
  const int itemCap = usable / flow.lineHeight < 1 ? 1 : usable / flow.lineHeight;

  flow.blocks.reserve(static_cast<size_t>(model.count * 2 + (model.todo ? 2 : 0)));
  for (int phase = 0; phase < (model.todo ? 2 : 1); ++phase) {
    bool sectionAdded = false;
    for (int i = 0; i < model.count; i++) {
      if (model.todo && model.tasks[i].checked != bool(phase)) continue;
      if (model.todo && !dense && !sectionAdded) {
        Block section;
        section.item = -1;
        section.section = true;
        section.height = 44;
        section.lines.reserve(1);
        section.lines.push_back(phase ? tr(STR_TASK_COMPLETED) : tr(STR_TASK_PENDING_PLURAL));
        flow.blocks.push_back(std::move(section));
        sectionAdded = true;
      }
      const std::string text = model.tasks[i].text != nullptr ? model.tasks[i].text : "";
      if (model.page) {
        const Broken broken = breakWords(target, text, flow.textWidth, 1000, flow.body, true);
        for (size_t l = 0; l < broken.lines.size(); l++) {
          Block line;
          line.item = i;
          line.lines.push_back(broken.lines[l]);
          line.height = flow.lineHeight;
          line.paragraphStart = (l == 0);
          flow.blocks.push_back(std::move(line));
        }
        continue;
      }
      // A run of letters wider than the row (a pasted link) is split at the
      // body cut. Shrinking the whole list to fit one link was the old answer.
      Broken broken = breakWords(target, text, flow.textWidth, 1000, flow.body, true);
      for (size_t start = 0; start < broken.lines.size(); start += itemCap) {
        Block row;
        row.item = i;
        const size_t end = std::min(broken.lines.size(), start + itemCap);
        row.lines.reserve(end - start);
        for (size_t line = start; line < end; ++line) row.lines.push_back(std::move(broken.lines[line]));
        const int textHeight = static_cast<int>(row.lines.size()) * flow.lineHeight + (dense ? 16 : 2 * kItemPad);
        row.height = static_cast<int16_t>(std::max(dense        ? 63
                                                   : model.todo ? 80
                                                                : kMinRow,
                                                   textHeight) +
                                          (model.todo && !dense ? 12 : 0));
        flow.blocks.push_back(std::move(row));
      }
    }
  }
  return flow;
}

// Greedy: a block starts a new page when it would not fit what is left. A
// paragraph's gap is dropped at the top of a page, where it would only push the
// first line down.
//
// And a paragraph never leaves ONE line behind at a break, at either end. A
// lone first line at the foot of a page reads as a sentence that stops; a lone
// last line at the top of the next reads as a stray. So the break moves back:
// to the paragraph's start when only its first line would remain, or one line
// earlier when only its last would carry over -- provided the page it is
// moving off keeps something of its own.
std::vector<int> pageStartsIn(const NoteFlow& flow, const int height) {
  const int count = static_cast<int>(flow.blocks.size());
  const auto paragraphOf = [&flow, count](const int i, int& begin, int& end) {
    begin = i;
    while (begin > 0 && !flow.blocks[static_cast<size_t>(begin)].paragraphStart) begin--;
    end = i + 1;
    while (end < count && !flow.blocks[static_cast<size_t>(end)].paragraphStart) end++;
  };
  std::vector<int> starts;
  starts.reserve(flow.blocks.size() + 1);
  starts.push_back(0);
  int y = 0;
  int i = 0;
  while (i < count) {
    const Block& block = flow.blocks[static_cast<size_t>(i)];
    const int gap = (block.paragraphStart && y > 0) ? flow.paragraphGap : 0;
    const int required = block.height + (block.section && i + 1 < count ? flow.blocks[i + 1].height : 0);
    if (y > 0 && y + gap + required > height) {
      int at = i;
      // Notes only. A list's items are blocks with no paragraphs, so the whole
      // list read as ONE paragraph and the rule moved a page's last row onto
      // the next page although it had room.
      if (flow.prose && !block.paragraphStart) {
        int begin = 0;
        int end = 0;
        paragraphOf(i, begin, end);
        if (i - begin == 1) {
          at = begin;  // only the first line would stay behind
        } else if (end - i == 1 && i - begin >= 3) {
          at = i - 1;  // only the last line would carry over
        }
        if (at <= starts.back()) at = i;  // never empty the page it leaves
      }
      starts.push_back(at);
      y = 0;
      i = at;
      continue;
    }
    y += gap + block.height;
    i++;
  }
  return starts;
}

struct NotePlan {
  NoteFlow flow;
  std::vector<int> starts;
  int16_t usable = 0;  // the band height the pages were cut against
};

NotePlan planNote(const fui::DrawTarget& target, const NoteModel& model, const fui::Rect& band) {
  NotePlan plan;
  plan.flow = flowNote(target, model, band);
  plan.usable = static_cast<int16_t>(band.height - (model.todo && !model.asleep ? 0 : pageLabelReserve(target)));
  plan.starts = pageStartsIn(plan.flow, plan.usable);
  return plan;
}

void noteRows(toybox::Screen& screen, const NoteModel& model, const fui::Rect& band, const NotePlan& plan) {
  if (model.count == 0) {
    centredNotice(
        screen, band,
        model.page ? tr(STR_UI_THIS_NOTE_IS_EMPTY_TAP_ADD_C33BB8) : tr(STR_UI_NOTHING_ON_THIS_LIST_TAP_ADD_026CBE));
    return;
  }
  const NoteFlow& flow = plan.flow;
  size_t page = 0;
  for (size_t p = 0; p < plan.starts.size(); p++) {
    if (plan.starts[p] <= model.firstVisible) page = p;
  }
  const int begin = plan.starts[page];
  const int end = page + 1 < plan.starts.size() ? plan.starts[page + 1] : static_cast<int>(flow.blocks.size());

  // Rows share the page when the whole list already fits on one, so three items
  // are not three lines over a hole. Capped, and never while paging, so a row
  // does not change size under the finger because the list grew past the fold.
  int16_t grow = 0;
  if (!model.todo && !model.page && plan.starts.size() == 1 && end > begin) {
    int total = 0;
    for (int i = begin; i < end; i++) total += flow.blocks[i].height;
    const int share = (plan.usable - total) / (end - begin);
    if (share > 0) grow = static_cast<int16_t>(share < kGrowCap ? share : kGrowCap);
  }

  int16_t y = band.y;
  for (int i = begin; i < end; i++) {
    const Block& block = flow.blocks[i];
    if (model.page) {
      if (block.paragraphStart && y > band.y) {
        if (flow.ruled)
          screen.target().line({band.x, int16_t(y + flow.paragraphGap / 2)},
                               {band.right(), int16_t(y + flow.paragraphGap / 2)}, 1, dashboard::ink);
        y = static_cast<int16_t>(y + flow.paragraphGap);
      }
      drawLines(screen, fui::makeRect(band.x, y, flow.textWidth, flow.lineHeight), block.lines, flow.body, false);
      y = static_cast<int16_t>(y + block.height);
      continue;
    }
    if (block.section) {
      dashboard::text(screen, fui::makeRect(band.x, y, band.width, 34), block.lines[0].c_str(), 1,
                      fui::TextAlign::Left);
      y += block.height;
      continue;
    }
    const Task& task = model.tasks[block.item];
    const fui::Rect row = fui::makeRect(
        band.x, y, band.width, static_cast<int16_t>(block.height + grow - (model.todo && model.asleep ? 12 : 0)));
    const int16_t textHeight = static_cast<int16_t>(static_cast<int>(block.lines.size()) * flow.lineHeight);
    const int16_t textTop = static_cast<int16_t>(row.y + (row.height - textHeight) / 2);
    // The box sits on the FIRST line, the way every checklist sets a wrapped
    // item: centred on the row it reads as belonging to the middle of the
    // sentence, and for a one-line row the two are the same place anyway.
    const int16_t boxTop = static_cast<int16_t>(textTop + flow.lineHeight / 2 - kBoxSide / 2);
    dashboard::frame(screen, row, 2);
    if (model.todo)
      dashboard::check(screen, fui::makeRect(row.x + 14, boxTop, kBoxSide, kBoxSide), task.checked);
    else
      tickBox(screen, fui::makeRect(row.x + 6, boxTop, kBoxSide, kBoxSide), task.checked);
    const int16_t textX = static_cast<int16_t>(row.x + kBoxSide + toybox::kGutter + (model.todo ? 16 : 0));
    drawLines(screen, fui::makeRect(textX, textTop, flow.textWidth, textHeight), block.lines, flow.body, task.checked);
    if (!model.asleep) rowHit(screen, row, ActionToggleTask, block.item);
    y = static_cast<int16_t>(y + row.height + (model.todo && model.asleep ? 12 : 0));
  }
}

}  // namespace

namespace {
// The strip under the band on a list: what is left, and a bar. It is the first
// thing the eye lands on after the name, and it is what the screen is FOR --
// the rows answer "which", the strip answers "how much".
// The strip sits in EQUAL air: the gap from the chrome down to the bar is the
// same as the gap from its rule down to the first row. It used to start at
// kBodyTop and keep the body's own 36px inset above it as well as a gutter
// below, which put a 14px bar inside 90px of page.
int16_t stripSpace(const NoteModel& model) {
  if (model.page || model.total <= 0) return 0;
  // The rows begin immediately under the rule and NO gap is added: a row is
  // taller than its tick box and centres it, so the row's own padding is
  // already the air below the rule -- about the same as the air above the bar,
  // which is what makes the strip sit in an even band. Adding a gutter here too
  // counted that space twice and pushed the first item a third of a page down.
  return 144;
}

fui::Rect noteBandFor(const fui::DeviceContext& device, const NoteModel& model) {
  if (!model.todo && !model.asleep && model.page) {
    const auto b = pixelui::geometry(device).body.inset(fui::Insets{4, 12, 0, 12});
    return fui::makeRect(b.x, b.y + 126, b.width, b.height - 252);
  }
  if (model.todo && !model.asleep) {
    const auto body = todoBody(device);
    const int top = model.freeTasks ? 0 : 40;
    return fui::makeRect(body.x, body.y + top, body.width, body.height - top - 120);
  }
  const int16_t width = static_cast<int16_t>(device.width - 2 * toybox::kMargin);
  // Asleep there is no footer, so the band runs to where the footer's top
  // edge would have been at the page's foot: rows plus the gutter that
  // separated them from it.
  const int16_t footerY = model.asleep ? static_cast<int16_t>(device.height - toybox::kMargin + toybox::kGutter * 2)
                                       : static_cast<int16_t>(device.height - toybox::kMargin - kFooterHeight);
  const int16_t top = static_cast<int16_t>(kBodyTop + stripSpace(model));
  return fui::makeRect(toybox::kMargin, top, width, static_cast<int16_t>(footerY - toybox::kGutter * 2 - top));
}

// "3 LEFT" and a bar on one line, with a rule under it closing the block off
// from the rows. Drawn against the page rather than on a slab: a second black
// band under the header would fight the header for the top of the screen.
void progressStrip(toybox::Screen& screen, const fui::Rect& band, const NoteModel& model) {
  const auto panel = fui::makeRect(band.x, kBodyTop, band.width, 126);
  dashboard::frame(screen, panel, 3);
  dashboard::text(screen, fui::makeRect(panel.x + 12, panel.y + 8, panel.width * 2 / 3 - 24, 28), tr(STR_TODO_PROGRESS),
                  3, fui::TextAlign::Left);
  char label[40];
  std::snprintf(label, sizeof(label), "%d/%d", model.done, model.total);
  dashboard::digits(screen, fui::makeRect(panel.x + 12, panel.y + 42, panel.width * 2 / 3 - 24, 48), label);
  std::snprintf(label, sizeof(label), "%d", model.total - model.done);
  dashboard::digits(screen, fui::makeRect(panel.x + panel.width * 2 / 3, panel.y + 8, panel.width / 3, 46), label);
  dashboard::text(screen, fui::makeRect(panel.x + panel.width * 2 / 3, panel.y + 58, panel.width / 3, 30),
                  tr(STR_TODO_LEFT), 3);
  dashboard::bar(screen, fui::makeRect(panel.x + 12, panel.bottom() - 24, panel.width - 24, 14), model.done,
                 model.total);
}
}  // namespace

std::vector<int> notePageStarts(const fui::DrawTarget& target, const fui::DeviceContext& device,
                                const NoteModel& model) {
  return planNote(target, model, noteBandFor(device, model)).starts;
}

int notePageOfItem(const fui::DrawTarget& target, const fui::DeviceContext& device, const NoteModel& model,
                   const int item) {
  const NotePlan plan = planNote(target, model, noteBandFor(device, model));
  int first = 0;
  for (size_t b = 0; b < plan.flow.blocks.size(); b++) {
    if (plan.flow.blocks[b].item == item) {
      first = static_cast<int>(b);
      break;
    }
  }
  int page = 0;
  for (size_t p = 0; p < plan.starts.size(); p++) {
    if (plan.starts[p] <= first) page = static_cast<int>(p);
  }
  return page;
}

void buildNote(toybox::Screen& screen, const NoteModel& model) {
  if (model.todo && !model.asleep) {
    todoChrome(screen, model.freeTasks ? tr(STR_TODO_FREE_TASKS) : model.title, !model.freeTasks);
    const auto body = todoBody(screen.device());
    if (!model.freeTasks) {
      char breadcrumb[160];
      std::snprintf(breadcrumb, sizeof(breadcrumb), "%s / %s", tr(STR_TODO_MY_LISTS), model.title);
      dashboard::prose(screen, fui::makeRect(body.x + 4, body.y, body.width - 100, 32), breadcrumb,
                       fui::TextAlign::Left);
      char value[32];
      std::snprintf(value, sizeof(value), "%d / %d", model.done, model.total);
      dashboard::text(screen, fui::makeRect(body.right() - 92, body.y, 88, 32), value, 0);
    }
    const auto band = noteBandFor(screen.device(), model);
    const auto plan = planNote(screen.target(), model, band);
    noteRows(screen, model, band, plan);
    int page = 0;
    for (size_t n = 1; n < plan.starts.size(); ++n)
      if (plan.starts[n] <= model.firstVisible) page = n;
    todoPager(screen, fui::makeRect(body.x, body.bottom() - 120, body.width, 48), page + 1, plan.starts.size());
    const int width = model.anyDone ? (body.width - 12) / 2 : body.width;
    const auto add = fui::makeRect(body.x, body.bottom() - 64, width, 64);
    if (model.anyDone) {
      dashboard::button(screen, add, tr(STR_ADD_TASK), ActionAddLine, 0, true);
      dashboard::button(screen, fui::makeRect(add.right() + 12, add.y, width, 64), tr(STR_UI_CLEAR_DONE_3EBB04),
                        ActionClearDone);
    } else
      dashboard::button(screen, add, tr(STR_ADD_TASK), ActionAddLine, 0, true, &extra_dashboard_add_32, 1);
    return;
  }
  if (!model.todo && !model.asleep && model.page) {
    pixelui::begin(screen);
    const auto geom = pixelui::geometry(screen.device());
    pixelui::Header header;
    header.title = model.title;
    header.back = screen.device().hasTouch;
    header.titleReserve = 72;
    pixelui::header(screen.target(), screen.device(), header);
    backui::publish(header.back, geom.back);
    const auto gear = fui::makeRect(geom.header.right() - 64, geom.header.y + (geom.header.height - 48) / 2, 48, 48);
    screen.target().fill(gear, dashboard::paper);
    dashboard::icon(screen, gear.inset(fui::Insets{8, 8, 8, 8}), icon_go_settings_32);
    dashboard::hit(screen, gear, ActionMenu);
    const auto b = geom.body.inset(fui::Insets{4, 4, 0, 4});
    const auto hero = fui::makeRect(b.x, b.y, b.width, 102);
    dashboard::frame(screen, hero, 2);
    readingui::text(screen, fui::makeRect(hero.x + 12, hero.y + 8, hero.width - 24, 58), model.title,
                    readingui::kReadingFont, 2);
    readingui::caps(screen, fui::makeRect(hero.x + 12, hero.y + 68, hero.width - 24, 28), model.updated, 3);
    const auto band = noteBandFor(screen.device(), model);
    const auto plan = planNote(screen.target(), model, band);
    noteRows(screen, model, band, plan);
    int page = 0;
    for (size_t n = 1; n < plan.starts.size(); ++n)
      if (plan.starts[n] <= model.firstVisible) page = n;
    todoPager(screen, fui::makeRect(b.x, b.bottom() - 116, b.width, 44), page + 1, plan.starts.size());
    dashboard::button(screen, fui::makeRect(b.x, b.bottom() - 64, b.width, 64), tr(STR_RT_ADD_TEXT), ActionAddLine, 0,
                      true, &extra_dashboard_add_32, 1);
    return;
  }
  chrome(screen, model.title, nullptr, model.menuIcon, toybox::kUiFont);
  const fui::DeviceContext& device = screen.device();
  const int16_t width = pageWidth(device);
  const int16_t footerY = static_cast<int16_t>(device.height - toybox::kMargin - kFooterHeight);
  const fui::Rect band = noteBandFor(device, model);
  if (stripSpace(model) > 0) progressStrip(screen, band, model);

  noteRows(screen, model, band, planNote(screen.target(), model, band));
  pageLabel(screen, band, model.pageLabel);
  if (model.asleep) return;

  // ADD keeps the left edge, the fork-wide home for a primary action and the
  // pixel a thumb learns. CLEAR DONE appears only when there is something to
  // clear, and it appears on the RIGHT, so the control that removes lines never
  // occupies the pixels ADD had a moment ago.
  if (model.anyDone && !model.page) {
    const int16_t half = static_cast<int16_t>((width - toybox::kGutter) / 2);
    footerButton(screen, fui::makeRect(toybox::kMargin, footerY, half, kFooterHeight), tr(STR_UI_ADD_F94604),
                 ActionAddLine, false);
    footerButton(
        screen,
        fui::makeRect(static_cast<int16_t>(toybox::kMargin + half + toybox::kGutter), footerY, half, kFooterHeight),
        tr(STR_UI_CLEAR_DONE_3EBB04), ActionClearDone, true);
  } else {
    footerButton(screen, fui::makeRect(toybox::kMargin, footerY, width, kFooterHeight), tr(STR_UI_ADD_F94604),
                 ActionAddLine, false);
  }
}

// --- The menu ------------------------------------------------------------

void buildConfirm(toybox::Screen& screen, const ConfirmModel& model) {
  if (model.reference) {
    const auto b = readingui::begin(screen, tr(STR_RT_OPTIONS));
    const bool wide = b.width > b.height;
    readingui::text(screen, fui::makeRect(b.x + 12, b.y + 20, b.width - 24, wide ? 120 : 300), model.prose, 4, 8);
    dashboard::button(screen, fui::makeRect(b.x, b.y + (wide ? 160 : b.height - 60), b.width, 58),
                      tr(STR_UI_DELETE_IT_619935), ActionDelete);
    dashboard::button(screen, fui::makeRect(b.x, b.y + (wide ? 270 : 410), b.width, wide ? 64 : 88),
                      tr(STR_UI_KEEP_IT_BCFE52), ActionDismiss, 0, true);
    return;
  }
  chrome(screen, model.title, nullptr, model.menuIcon, toybox::kBodyFont);
  const fui::DeviceContext& device = screen.device();
  // What the delete costs, because the only thing a person can do about a note
  // they did not mean to delete is not delete it.
  sheetProse(screen, sheetBand(device), model.prose);

  // KEEP takes the left, filled: the pixels ADD and the safe action occupy on
  // every other screen. DELETE takes the right, outlined, where CLEAR DONE is
  // -- the side of the bar that takes things away.
  const fui::Rect footer = footerBand(device);
  const int16_t half = static_cast<int16_t>((footer.width - toybox::kGutter) / 2);
  footerButton(screen, fui::makeRect(footer.x, footer.y, half, footer.height), tr(STR_UI_KEEP_IT_BCFE52), ActionDismiss,
               false);
  footerButton(screen,
               fui::makeRect(static_cast<int16_t>(footer.x + half + toybox::kGutter), footer.y, half, footer.height),
               tr(STR_UI_DELETE_IT_619935), ActionDelete, true);
}

fui::Rect buildPhone(toybox::Screen& screen, const PhoneModel& model) {
  chrome(screen, model.title, nullptr, model.menuIcon, toybox::kBodyFont);
  const fui::DeviceContext& device = screen.device();
  const int16_t width = pageWidth(device);
  const int16_t footerY = static_cast<int16_t>(device.height - toybox::kMargin - kFooterHeight);

  const int16_t lineHeight = screen.target().lineHeight(toybox::kTileFont);
  const fui::Rect caption =
      fui::makeRect(toybox::kMargin, static_cast<int16_t>(kBodyTop + toybox::kGutter), width, lineHeight);
  fittedLine(screen, caption, tr(STR_UI_POINT_YOUR_PHONE_CAMERA_HERE_26B2E2), fui::TextAlign::Center,
             toybox::kTileFont);

  // The code, the address under it, and the state under that, stacked from the
  // caption down rather than from the footer up: the block grows downward into
  // space that is empty, instead of upward into the button.
  const int16_t top = static_cast<int16_t>(caption.y + caption.height + toybox::kGutter * 2);
  // The code takes about half the panel, not all of it. It used to grow into
  // every pixel left over, which pushed the address -- the one string somebody
  // may have to read off the glass and type into a browser -- to the bottom in
  // the smallest type on the screen, under a code they had already scanned.
  const int16_t room = static_cast<int16_t>(footerY - toybox::kGutter * 2 - top - lineHeight * 5);
  int16_t side = room < width ? room : width;
  if (side > 300) side = 300;
  if (side < 120) side = 120;
  const fui::Rect qr = fui::makeRect(static_cast<int16_t>((device.width - side) / 2), top, side, side);

  const fui::Rect url =
      fui::makeRect(toybox::kMargin, static_cast<int16_t>(qr.y + qr.height + toybox::kGutter), width, lineHeight);
  fittedLine(screen, url, model.readable, fui::TextAlign::Center, toybox::kTileFont);

  // The state line says whether anything has arrived. A screen whose whole
  // promise is "type over there and it appears here" has to answer "did it".
  const fui::Rect state =
      fui::makeRect(toybox::kMargin, static_cast<int16_t>(url.y + url.height + toybox::kGutter), width, lineHeight);
  fittedLine(screen, state,
             model.saved ? tr(STR_UI_SAVED_FROM_YOUR_PHONE_EF2A7D) : tr(STR_UI_WAITING_FOR_YOUR_PHONE_16F7BD),
             fui::TextAlign::Center, toybox::kTileFont);

  footerButton(screen, fui::makeRect(toybox::kMargin, footerY, width, kFooterHeight), tr(STR_UI_DONE_E782E7),
               ActionDismiss, false);
  return qr;
}

void buildNotice(toybox::Screen& screen, const ConfirmModel& model) {
  chrome(screen, model.title, nullptr, nullptr, toybox::kBodyFont);
  const fui::DeviceContext& device = screen.device();
  sheetProse(screen, sheetBand(device), model.prose);
  footerButton(screen, footerBand(device), tr(STR_UI_BACK_587EAC), ActionDismiss, false);
}

void buildMenu(toybox::Screen& screen, const MenuModel& model) {
  if (model.todo && model.isList) {
    todoChrome(screen, tr(STR_TODO_LIST_OPTIONS), false, ActionDismiss);
    const auto body = todoBody(screen.device());
    const bool wide = body.width > body.height;
    const auto hero = fui::makeRect(body.x, body.y, wide ? body.width * 2 / 5 : body.width, 80);
    dashboard::frame(screen, hero, 2);
    dashboard::icon(screen, fui::makeRect(hero.x + 16, hero.y + 14, 52, 52), extra_list_32);
    dashboard::text(screen, fui::makeRect(hero.x + 94, hero.y, hero.width - 108, hero.height), model.title, 1,
                    fui::TextAlign::Left);
    const int left = wide ? hero.right() + 16 : body.x, width = body.right() - left,
              top = wide ? body.y : hero.bottom() + 24, height = wide ? 62 : 84;
    const char* labels[] = {
        tr(STR_TODO_RENAME_LIST), tr(STR_UI_MAKE_IT_A_NOTE_A34558),
        model.shownAsleep ? tr(STR_UI_TAKE_OFF_SLEEP_SCREEN_FB76EA) : tr(STR_UI_PUT_ON_SLEEP_SCREEN_A7FB15),
        tr(STR_TODO_DELETE_LIST)};
    static constexpr fui::ActionId actions[] = {ActionRename, ActionSwitchKind, ActionShowAsleep, ActionDelete};
    static constexpr const fui::BitmapRef* art[] = {&compactart::Pencil, &compactart::Convert, &compactart::ListMoon,
                                                    &compactart::Trash};
    for (int n = 0; n < 4; ++n) {
      const auto row = fui::makeRect(left, top + n * height + (n > 1 ? 20 : 0) + (n == 3 ? 20 : 0), width,
                                     height + (n == 1 ? 20 : 0));
      dashboard::frame(screen, row, 2);
      compactart::draw(screen, fui::makeRect(row.x + 18, row.y + (height - 48) / 2, 48, 48), *art[n]);
      dashboard::text(
          screen, fui::makeRect(row.x + 88, row.y + (n == 1 ? 4 : 0), row.width - 130, n == 1 ? height / 2 : height),
          labels[n], 0, fui::TextAlign::Left);
      if (n == 1)
        dashboard::prose(screen, fui::makeRect(row.x + 88, row.y + height / 2, row.width - 130, height / 2 - 4),
                         tr(STR_TODO_CONVERT_WARNING), fui::TextAlign::Left, 2);
      dashboard::chevron(screen.target(), fui::makeRect(row.right() - 36, row.y, 26, row.height));
      dashboard::hit(screen, row, actions[n]);
    }
    dashboard::button(screen, fui::makeRect(body.x, body.bottom() - 60, wide ? hero.width : body.width, 60),
                      tr(STR_TODO_BACK_LIST), ActionDismiss);
    return;
  }
  const auto b = readingui::begin(screen, tr(STR_RT_OPTIONS));
  const bool wide = b.width > b.height;
  readingui::caps(screen, fui::makeRect(b.x + 8, b.y, b.width - 16, 44), model.title, 0, 2);
  const int h = wide ? 64 : 88;
  const int sleepH = wide ? 104 : 144, gap = 12;
  const int stackH = h * 2 + sleepH + gap * 2;
  const int top = b.y + 52 + std::max(0, (b.height - 52 - stackH) / 2);
  const auto rename = fui::makeRect(b.x, top, b.width, h);
  const auto sleep = fui::makeRect(b.x, rename.bottom() + gap, b.width, sleepH);
  const auto remove = fui::makeRect(b.x, sleep.bottom() + gap, b.width, h);
  dashboard::frame(screen, rename, 2);
  compactart::draw(screen, fui::makeRect(rename.x + 18, rename.y + (h - 44) / 2, 44, 44), compactart::Pencil);
  readingui::caps(screen, fui::makeRect(rename.x + 90, rename.y, rename.width - 130, rename.height),
                  tr(STR_UI_RENAME_475608), 1);
  dashboard::chevron(screen.target(), fui::makeRect(rename.right() - 34, rename.y, 28, rename.height));
  dashboard::hit(screen, rename, ActionRename);
  dashboard::frame(screen, sleep, 2);
  compactart::draw(screen, fui::makeRect(sleep.x + 18, sleep.y + (sleep.height - 44) / 2, 44, 44),
                   compactart::ListMoon);
  readingui::caps(screen, fui::makeRect(sleep.x + 90, sleep.y + 10, sleep.width - 134, 42),
                  model.shownAsleep ? tr(STR_RT_STOP_SLEEP_NOTE) : tr(STR_RT_USE_SLEEP_NOTE), 0, 2);
  readingui::text(screen, fui::makeRect(sleep.x + 90, sleep.y + 58, sleep.width - 134, sleep.height - 64),
                  tr(STR_RT_SLEEP_NOTE_HELP), 4, 3);
  dashboard::chevron(screen.target(), fui::makeRect(sleep.right() - 34, sleep.y, 28, sleep.height));
  dashboard::hit(screen, sleep, ActionShowAsleep);
  dashboard::frame(screen, remove, 2);
  compactart::draw(screen, fui::makeRect(remove.x + 18, remove.y + (h - 44) / 2, 44, 44), compactart::Trash);
  readingui::caps(screen, fui::makeRect(remove.x + 90, remove.y, remove.width - 130, remove.height),
                  tr(STR_UI_DELETE_NOTE_12BEF2), 1);
  dashboard::chevron(screen.target(), fui::makeRect(remove.right() - 34, remove.y, 28, remove.height));
  dashboard::hit(screen, remove, ActionDelete);
}

}  // namespace notesui
