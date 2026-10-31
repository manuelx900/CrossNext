#include "InterceptScreens.h"

#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "../ui/FitText.h"
#include "../ui/GameGuide.h"
#include "../ui/RequiredResources.h"
#include "InterceptArt.h"
namespace interceptui {
namespace fui = freeink::ui;
namespace {
void art(toybox::Screen& s, const Model& m, fui::Rect r, unsigned id) {
  if (m.assets) m.assets->draw(s.target(), r, id);
}
void scene(toybox::Screen& s, const Model& m, fui::Rect r, int n) {
  art(s, m, r, unsigned(std::clamp(n, 0, int(interceptart::SCENE_COUNT) - 1)));
}
void prose(toybox::Screen& s, fui::Rect r, const char* copy, int lines = 8, int font = 4) {
  fittext::draw(s.target(), r, copy, font, lines);
}
void guided(toybox::Screen& s, fui::Rect r, const char* copy, int lines, fui::TextAlign align, int font) {
  fittext::draw(s.target(), r, copy, font, lines, align);
}
void menu(toybox::Screen& s, fui::Rect r, const char* label, fui::ActionId action, bool primary, gamepanel::Mark mark,
          int value = 0) {
  gamepanel::row(s, r, label, action, value, mark, primary, nullptr, true);
}
}  // namespace
void build(toybox::Screen& s, const Model& m, Work& w) {
  pixelui::begin(s);
  pixelui::Header h;
  h.title = tr(STR_INTERCEPT_NAME);
  h.back = true;
  pixelui::header(s, h);
  backui::publish(true, pixelui::geometry(s.device()).back);
  if (m.assets && !m.assets->ready()) {
    requiredresources::build(s, "/apps/intercept/", Back, w.button);
    return;
  }
  auto b = s.body();
  if (m.failed) {
    prose(s, fui::makeRect(b.x, b.y, b.width, 38), tr(STR_TOOLS_SAVE_FAILED), 2);
    b.y += 44;
    b.height -= 44;
  }
  if (!m.state || m.state->node >= intercept::nodeCount) return;
  const auto& state = *m.state;
  const auto& node = intercept::nodes[state.node];
  const bool wide = b.width > b.height;
  if (m.view == View::About) {
    w.pages = 5;
    const int page = std::clamp(m.page, 0, w.pages - 1);
    const auto content = fui::makeRect(b.x, b.y, b.width, b.height - 64);
    if (page < 4) {
      static constexpr StrId titles[] = {StrId::STR_MYST_GUIDE_OBJECTIVE, StrId::STR_MYST_GUIDE_YOU,
                                         StrId::STR_MYST_GUIDE_HOOPER, StrId::STR_MYST_GUIDE_HARRIS};
      static constexpr StrId roles[] = {StrId::STR_MYST_GUIDE_SETTING, StrId::STR_MYST_GUIDE_YOU_ROLE,
                                        StrId::STR_MYST_GUIDE_HOOPER_ROLE, StrId::STR_MYST_GUIDE_HARRIS_ROLE};
      static constexpr StrId copy[] = {StrId::STR_MYST_GUIDE_OBJECTIVE_COPY, StrId::STR_MYST_GUIDE_YOU_COPY,
                                       StrId::STR_MYST_GUIDE_HOOPER_COPY, StrId::STR_MYST_GUIDE_HARRIS_COPY};
      static constexpr int scenes[] = {0, 1, 11, 2};
      constexpr int artPercent = 43;
      const auto picture = wide ? fui::makeRect(content.x, content.y, content.width * artPercent / 100, content.height)
                                : fui::makeRect(content.x, content.y, content.width, content.height * artPercent / 100);
      scene(s, m, picture, scenes[page]);
      const auto panel =
          wide ? fui::makeRect(picture.right() + 20, content.y, content.right() - picture.right() - 20, content.height)
               : fui::makeRect(content.x, picture.bottom() + 16, content.width,
                               content.bottom() - picture.bottom() - 16);
      dashboard::frame(s, panel);
      dashboard::text(s, fui::makeRect(panel.x + 14, panel.y + 12, panel.width - 28, 42), I18N.get(titles[page]), 1,
                      fui::TextAlign::Left);
      guided(s, fui::makeRect(panel.x + 14, panel.y + 58, panel.width - 28, 50), I18N.get(roles[page]), 2,
             fui::TextAlign::Left, 0);
      prose(s, fui::makeRect(panel.x + 14, panel.y + 120, panel.width - 28, panel.height - 132), I18N.get(copy[page]),
            12);
    } else {
      dashboard::text(s, fui::makeRect(content.x, content.y, content.width, 42), tr(STR_MYST_HOW), 1,
                      fui::TextAlign::Left);
      const int leftW = wide ? content.width * 56 / 100 : content.width;
      const int rowH = wide ? (content.height - 54) / 3 : 96;
      static constexpr StrId copy[] = {StrId::STR_MYST_GUIDE_CHOICES, StrId::STR_MYST_PAGES, StrId::STR_MYST_SAVES};
      for (int n = 0; n < 3; ++n) {
        const auto row = fui::makeRect(content.x, content.y + 54 + n * rowH, leftW, rowH - 8);
        guideui::badge(s, fui::makeRect(row.x + 4, row.y + 8, 34, 34), n + 1);
        prose(s, fui::makeRect(row.x + 52, row.y + 4, row.width - 60, row.height - 8), I18N.get(copy[n]), 4);
      }
      const auto credits =
          wide ? fui::makeRect(content.x + leftW + 20, content.y + 54, content.width - leftW - 20, content.height - 54)
               : fui::makeRect(content.x, content.y + 54 + 3 * rowH, content.width, content.height - 54 - 3 * rowH);
      dashboard::frame(s, credits);
      prose(s, credits.inset({12, 14, 12, 14}), tr(STR_MYST_CREDIT_COPY), 10);
    }
    pageui::build(s, fui::makeRect(b.x, b.bottom() - 48, b.width, 44), page + 1, w.pages, Page, -1, Page, 1);
    return;
  }
  if (m.view == View::Restart) {
    const int left = wide ? b.width / 3 : b.width;
    const int heroH = wide ? b.height : b.height * 37 / 100;
    art(s, m, fui::makeRect(b.x + (left - 116) / 2, b.y + 30, 116, std::max(100, heroH - 80)), interceptart::RESTART);
    dashboard::text(s, fui::makeRect(b.x, b.y + heroH - 36, left, 48), tr(STR_INTERCEPT_NEW), 2);
    const auto panel = wide ? fui::makeRect(b.x + left + 18, b.y, b.width - left - 18, b.height)
                            : fui::makeRect(b.x, b.y + heroH + 30, b.width, b.height - heroH - 30);
    const auto warning = fui::makeRect(panel.x, panel.y, panel.width, panel.height - 154);
    dashboard::frame(s, warning);
    dashboard::text(s, fui::makeRect(warning.x + 12, warning.y + 24, warning.width - 24, 48), tr(STR_GUIDE_START_AGAIN),
                    1);
    guided(s, fui::makeRect(warning.x + 22, warning.y + 88, warning.width - 44, warning.height - 100),
           tr(STR_MYST_RESTART_COPY), 5, fui::TextAlign::Center, 4);
    menu(s, fui::makeRect(panel.x, panel.bottom() - 136, panel.width, 64), tr(STR_INTERCEPT_NEW), ConfirmNew,
         m.selected == 0, gamepanel::Mark::Restart);
    menu(s, fui::makeRect(panel.x, panel.bottom() - 62, panel.width, 62), tr(STR_CANCEL), Cancel, m.selected == 1,
         gamepanel::Mark::Back);
    return;
  }
  if (m.view == View::Menu) {
    const auto left = wide ? fui::makeRect(b.x, b.y, b.width / 2 - 10, b.height) : b;
    const int artH = wide ? left.height - 100 : std::min(276, int(b.height) * 43 / 100);
    scene(s, m, fui::makeRect(left.x, left.y, left.width, artH), 0);
    dashboard::text(s, fui::makeRect(left.x, left.y + artH + 12, left.width, 34), tr(STR_REDESIGN_BLETCHLEY), 1);
    dashboard::text(s, fui::makeRect(left.x, left.y + artH + 49, left.width, 30), tr(STR_REDESIGN_INTERACTIVE_MYSTERY),
                    0);
    const auto right = wide ? fui::makeRect(left.right() + 18, b.y, b.right() - left.right() - 18, b.height)
                            : fui::makeRect(b.x, left.y + artH + 98, b.width, b.bottom() - left.y - artH - 98);
    const int row = std::min(66, (int(right.height) - 72) / 3) - 4;
    const int groupTop = wide ? gamepanel::centeredBlock(right, 3 * row + 20 + 16 + 62).y : right.y;
    const char* labels[] = {state.choices ? tr(STR_INTERCEPT_CONTINUE) : tr(STR_WORDLE_PLAY), tr(STR_INTERCEPT_NEW),
                            tr(STR_MYST_GUIDE_TITLE)};
    const fui::ActionId ids[] = {Play, New, About};
    const gamepanel::Mark marks[] = {gamepanel::Mark::Play, gamepanel::Mark::Restart, gamepanel::Mark::Help};
    for (int n = 0; n < 3; ++n) {
      const auto box = fui::makeRect(right.x, groupTop + n * (row + 10), right.width, row);
      if (n == 2)
        dashboard::action(s, box, labels[n], ids[n], &extra_information_32, n == m.selected);
      else
        menu(s, box, labels[n], ids[n], n == m.selected, marks[n]);
    }
    guided(s, fui::makeRect(right.x + 8, wide ? groupTop + 3 * row + 36 : right.bottom() - 62, right.width - 16, 62),
           tr(STR_REDESIGN_INTERCEPT_TAGLINE), 3, fui::TextAlign::Center, 4);
    return;
  }
  const auto chapter = fui::makeRect(b.x, b.y, b.width, 42);
  dashboard::frame(s, chapter, pixelui::kBorder);
  dashboard::text(s, chapter, I18N.get(node.title), 1);
  b.y += 54;
  b.height -= 54;
  const bool ending = node.ending != intercept::Ending::None;
  const auto art = wide ? fui::makeRect(b.x, b.y, b.width * 38 / 100, b.height)
                        : fui::makeRect(b.x, b.y, b.width, std::min(238, int(b.height) * 42 / 100));
  scene(s, m, art, node.scene);
  const auto panel = wide ? fui::makeRect(art.right() + 14, b.y, b.right() - art.right() - 14, b.height)
                          : fui::makeRect(b.x, art.bottom() + 12, b.width, b.bottom() - art.bottom() - 12);
  int count = 0;
  for (unsigned n = 0; n < node.count; ++n)
    if (intercept::allowed(state, n)) ++count;
  const int rowH = wide ? 44 : 44, controls = ending ? 118 : count * (rowH + 6) + 34;
  const auto textBox = fui::makeRect(panel.x, panel.y, panel.width, panel.height - controls - 8);
  dashboard::frame(s, textBox, pixelui::kBorder);
  const auto narrative = textBox.inset({12, 12, 52, 12});
  w.pages = pagedtext::draw(s.target(), narrative, I18N.get(node.text), 4, m.page, w.paragraph);
  if (w.pages > 1)
    pageui::build(s, fui::makeRect(textBox.x + 8, textBox.bottom() - 44, textBox.width - 16, 36), m.page + 1, w.pages,
                  Page, -1, Page, 1);
  else if (ending)
    dashboard::text(s, fui::makeRect(textBox.x + 12, textBox.bottom() - 42, textBox.width - 24, 34), tr(STR_MYST_END),
                    0);
  if (ending) {
    menu(s, fui::makeRect(panel.x, panel.bottom() - 110, panel.width, 50), tr(STR_INTERCEPT_MENU), Cancel, true,
         gamepanel::Mark::Back);
    menu(s, fui::makeRect(panel.x, panel.bottom() - 52, panel.width, 52), tr(STR_INTERCEPT_NEW), New, false,
         gamepanel::Mark::Restart);
    return;
  }
  const auto heading = fui::makeRect(panel.x, panel.bottom() - controls, panel.width, 28);
  lightui::surface(s.target(), heading, dashboard::ink);
  dashboard::text(s, heading, tr(STR_REDESIGN_NEXT_MOVE), 0, fui::TextAlign::Center, true);
  int row = 0;
  for (unsigned n = 0; n < node.count; ++n)
    if (intercept::allowed(state, n)) {
      const auto r = fui::makeRect(panel.x, heading.bottom() + 6 + row * (rowH + 6), panel.width, rowH);
      const bool selected = s.frame().buttonSelected(Choice, n, row == m.selected);
      if (selected)
        lightui::surface(s.target(), r, dashboard::ink);
      else
        dashboard::frame(s, r, 2);
      dashboard::hit(s, r, Choice, n);
      s.target().line({int16_t(r.x + 42), r.y}, {int16_t(r.x + 42), r.bottom()}, 2,
                      selected ? dashboard::paper : dashboard::ink);
      char num[8];
      std::snprintf(num, sizeof(num), "%d", row + 1);
      dashboard::digits(s, fui::makeRect(r.x + 10, r.y + 8, 24, r.height - 16), num, selected);
      dashboard::text(s, fui::makeRect(r.x + 54, r.y + 4, r.width - 66, r.height - 8), I18N.get(node.choices[n].label),
                      0, fui::TextAlign::Left, selected);
      ++row;
    }
}
}  // namespace interceptui
