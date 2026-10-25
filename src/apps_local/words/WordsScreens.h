#pragma once
#include "../ui/PagedText.h"
#include "../ui/PixelMenu.h"
#include "WordsText.h"

namespace wordsui {
namespace fui = freeink::ui;
enum Action : int {
  Open = 1,
  Previous,
  Next,
  Review,
  ReviewHard,
  ReviewAll,
  FullList,
  Export,
  Reveal,
  Rate,
  Passage,
  Vocabulary,
  Languages,
  Dismiss,
  Help,
  Back,
  Settings,
  ReviewWord,
  Delete,
  ConfirmDelete,
  Cancel,
  ActionCount
};
enum class View : uint8_t { Home, List, WordList, Detail, Review, Guide, Waiting, Delete };
enum class ReviewKind : uint8_t { DueWords, Difficult, AllSaved, SingleWord };
constexpr int kListCapacity = 5;
constexpr int kWordListCapacity = 10;
constexpr int kGuidePages = 4;
constexpr bool isList(View view) { return view == View::List || view == View::WordList; }
constexpr int listCapacity(View view, int width, int height) {
  return view == View::WordList && width <= height ? kWordListCapacity : kListCapacity;
}
constexpr int reviewRating(int focus) { return focus + 1; }
struct Row {
  char id[33]{}, label[192]{}, source[224]{};
};
struct Model {
  ParagraphPages textCache;
  Row rows[kWordListCapacity];
  View view = View::Home;
  const char *term = "", *meaning = "", *sentence = "", *book = "", *language = "";
  const char* text = "";
  const char* filter = "";
  const char* message = "";
  ReviewKind review = ReviewKind::DueWords;
  int count = 0, total = 0, capacity = kListCapacity, page = 0, textPage = 0, textPages = 1, focus = 0;
  int saved = 0, pending = 0, due = 0, eligible = 0, difficult = 0, guidePage = 0, reviewIndex = 0, reviewTotal = 0;
  bool translation = false, readerContent = false;
  bool busy = false, answer = false, known = false, clockReady = true;
  char delays[4][32]{};
  int waitMinutes = 0;
  bool keyboardFocus = false;
  bool error = false;
};
inline void setListCapacity(Model& model, int capacity) {
  if (model.capacity == capacity) return;
  const int ordinal = model.page * model.capacity + (model.focus > 0 ? model.focus : 0);
  model.page = ordinal / capacity;
  model.focus = ordinal % capacity;
  model.capacity = capacity;
}
struct Geometry {
  fui::Rect buttons[ActionCount]{}, rows[kWordListCapacity]{}, text{}, before{}, after{};
};
fui::Rect textBox(const fui::DeviceContext&, const Model&);
int measureCard(fui::DrawTarget&, fui::Rect, Model&, pagedtext::Work&, int* used = nullptr,
                uint8_t* wordBreaks = nullptr, bool fitOnly = false);
void draw(fui::DrawTarget&, const fui::DeviceContext&, Model&, Geometry&, pagedtext::Work&,
          fui::DrawTarget* content = nullptr);
template <typename Screen>
void build(Screen& screen, Model& model, Geometry& g, pagedtext::Work& work, fui::DrawTarget* content = nullptr) {
  draw(screen.target(), screen.device(), model, g, work, content);
  if (model.error) {
    screen.frame().hit(g.buttons[Back], Back, 0, fui::InputTouch);
    return;
  }
  for (int i = 1; i < ActionCount; ++i)
    if ((!model.busy || i == Back) && !g.buttons[i].empty()) screen.frame().hit(g.buttons[i], i, 0, fui::InputTouch);
  if (!g.before.empty()) screen.frame().hit(g.before, Previous, 0, fui::InputTouch);
  if (!g.after.empty()) screen.frame().hit(g.after, Next, 0, fui::InputTouch);
  if (!model.busy && model.view == View::Review && model.answer)
    for (int n = 0; n < 4; ++n) screen.frame().hit(g.rows[n], Rate, reviewRating(n), fui::InputTouch);
  if (!model.busy && isList(model.view) && !*model.message)
    for (int i = 0; i < model.count; ++i) screen.frame().hit(g.rows[i], Open, i, fui::InputTouch);
}
}  // namespace wordsui
