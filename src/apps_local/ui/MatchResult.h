#pragma once
#include "FitText.h"
#include "GamePanel.h"
namespace matchresult {
namespace fui = freeink::ui;
enum class BadgePosition { BeforeArt, AfterArt };
inline void draw(toybox::Screen& s, fui::Rect body, const char* title, const char* winner, const char* badge,
                 const char* note, fui::BitmapRef art, fui::ActionId newGame, fui::ActionId review, fui::ActionId menu,
                 BadgePosition badgePosition = BadgePosition::BeforeArt) {
  const bool wide = body.width > body.height;
  const bool badgeAfterArt = badgePosition == BadgePosition::AfterArt;
  const auto hero = wide ? fui::makeRect(body.x, body.y, body.width / 2 - 12, body.height) : body;
  const auto controls =
      wide ? fui::makeRect(hero.right() + 24, body.y, body.width - hero.width - 24, body.height) : body;
  char capital[128];
  pixelui::copyCaps(capital, sizeof(capital), title);
  fittext::draw(s.target(), fui::makeRect(hero.x, hero.y + 8, hero.width, 68), capital, 2, 1, fui::TextAlign::Center);
  fittext::draw(s.target(), fui::makeRect(hero.x, hero.y + 80, hero.width, 42), winner, 1, 1, fui::TextAlign::Center);
  const int groupTop = gamepanel::centeredBlock(controls, 314).y;
  const int actionY = wide ? groupTop + 124 : controls.bottom() - 196;
  const auto badgeBox = fui::makeRect(wide ? controls.x : hero.x + 24,
                                      wide            ? groupTop
                                      : badgeAfterArt ? actionY - 108
                                                      : hero.y + 126,
                                      wide ? controls.width : hero.width - 48, 40);
  s.target().stroke(badgeBox, dashboard::ink, 3);
  pixelui::copyCaps(capital, sizeof(capital), badge);
  fittext::draw(s.target(), badgeBox.inset({0, 6, 0, 6}), capital, 0, 1, fui::TextAlign::Center);
  const int artY = hero.y + (badgeAfterArt ? 128 : 180);
  const int artBottom = wide ? hero.bottom() - 8 : badgeAfterArt ? badgeBox.y - 12 : actionY - 86;
  if (art.data)
    s.target().bitmap(fui::makeRect(hero.x + 16, artY, hero.width - 32, std::max(24, artBottom - artY)), art,
                      fui::BitmapMode::Contain, dashboard::ink);
  fittext::draw(s.target(), fui::makeRect(controls.x, wide ? badgeBox.bottom() + 12 : actionY - 62, controls.width, 56),
                note, 4, 2, fui::TextAlign::Center);
  gamepanel::row(s, fui::makeRect(controls.x, actionY, controls.width, 58), tr(STR_UI_NEW_GAME_F6F041), newGame, 0,
                 gamepanel::Mark::Play, true);
  gamepanel::row(s, fui::makeRect(controls.x, actionY + 66, controls.width, 58), tr(STR_C4_VIEW_BOARD), review, 0,
                 gamepanel::Mark::Grid);
  gamepanel::row(s, fui::makeRect(controls.x, actionY + 132, controls.width, 58), tr(STR_UI_BACK_TO_MENU_398BBB), menu,
                 0, gamepanel::Mark::Back);
  const int x = controls.x + 16, gridY = actionY + 66 + 9, homeY = actionY + 132 + 9;
  s.target().fill(fui::makeRect(x, gridY, 40, 40), dashboard::paper);
  for (int row = 0; row < 4; ++row)
    for (int col = 0; col < 4; ++col)
      if ((row + col) % 2 == 0) s.target().fill(fui::makeRect(x + col * 10, gridY + row * 10, 10, 10), dashboard::ink);
  s.target().stroke(fui::makeRect(x, gridY, 40, 40), dashboard::ink, 2);
  s.target().fill(fui::makeRect(x, homeY, 40, 40), dashboard::paper);
  s.target().line({int16_t(x + 2), int16_t(homeY + 19)}, {int16_t(x + 20), int16_t(homeY + 2)}, 3, dashboard::ink);
  s.target().line({int16_t(x + 20), int16_t(homeY + 2)}, {int16_t(x + 38), int16_t(homeY + 19)}, 3, dashboard::ink);
  s.target().line({int16_t(x + 6), int16_t(homeY + 16)}, {int16_t(x + 6), int16_t(homeY + 37)}, 3, dashboard::ink);
  s.target().line({int16_t(x + 34), int16_t(homeY + 16)}, {int16_t(x + 34), int16_t(homeY + 37)}, 3, dashboard::ink);
  s.target().line({int16_t(x + 6), int16_t(homeY + 37)}, {int16_t(x + 15), int16_t(homeY + 37)}, 3, dashboard::ink);
  s.target().line({int16_t(x + 25), int16_t(homeY + 37)}, {int16_t(x + 34), int16_t(homeY + 37)}, 3, dashboard::ink);
  s.target().stroke(fui::makeRect(x + 15, homeY + 24, 10, 13), dashboard::ink, 3);
}
}  // namespace matchresult
