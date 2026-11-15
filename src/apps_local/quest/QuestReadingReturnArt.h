#pragma once

#include "QuestAvatar.h"
#include "QuestEnemyArt.h"
#include "QuestReadingReturn.h"

namespace questreturn {
inline questenemy::Pose monsterPose(Phase phase) {
  switch (phase) {
    case Phase::Raise:
      return questenemy::Pose::Ready;
    case Phase::Swing:
      return questenemy::Pose::Weakened;
    case Phase::Impact:
      return questenemy::Pose::Defeated;
    case Phase::Idle:
    case Phase::Result:
      return questenemy::Pose::Current;
  }
  return questenemy::Pose::Current;
}
inline void letters(freeink::ui::DrawTarget& target, freeink::ui::Rect area, sdart::Pack& assets,
                    const quest::Profile& profile, Phase phase, uint32_t seed, questavatar::Framing framing,
                    sdart::Pack* rewards = nullptr, sdart::Pack* outfits = nullptr) {
  namespace fui = freeink::ui;
  int first = 0, step = 0;
  switch (phase) {
    case Phase::Raise:
      first = 0;
      step = 7;
      break;
    case Phase::Swing:
      first = 25;
      step = 11;
      break;
    case Phase::Impact:
      first = 60;
      step = 8;
      break;
    case Phase::Idle:
    case Phase::Result:
      return;
  }
  const auto sprite = questextra::figure(assets, rewards, profile.equipped[0], profile.gender, outfits);
  const auto hero = sprite.pack ? sprite.pack->image(sprite.ink) : sdart::Image{};
  if (area.empty() || !hero.height) return;
  const auto scene = framing == questavatar::Framing::Tall && area.height * 3 > area.width * 2
                         ? questavatar::groundedSceneRect(area)
                         : questavatar::sceneRect(area);
  if (scene.empty()) return;
  const auto layout = questenemy::arrange(hero);
  const int fromX = layout.hero.x + layout.hero.width * 3 / 4;
  const int fromY = layout.hero.y + layout.hero.height * 48 / 100;
  const int toX = layout.enemy.x + layout.enemy.width * 55 / 100;
  const int toY = layout.enemy.y + layout.enemy.height * (phase == Phase::Impact ? 82 : 42) / 100;
  const auto point = [&](int x, int y) {
    return fui::Point{int16_t(scene.x + x * scene.width / 480), int16_t(scene.y + y * scene.height / 320)};
  };
  const auto previous = target.clipRect();
  const auto clipped = fui::makeRect(std::max(area.x, previous.x), std::max(area.y, previous.y),
                                     std::min(area.right(), previous.right()) - std::max(area.x, previous.x),
                                     std::min(area.bottom(), previous.bottom()) - std::max(area.y, previous.y));
  if (clipped.empty() || !target.setClipRect(clipped)) return;
  seed = (seed ^ 0x9e3779b9u) | 1u;
  for (int n = 0; n < 6; ++n) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    const int progress = std::min(100, first + n * step);
    const int x = fromX + (toX - fromX) * progress / 100;
    const int y = fromY + (toY - fromY) * progress / 100 - progress * (100 - progress) * 36 / 2500 + (n % 2 ? -12 : 12);
    const auto center = point(x, y);
    fui::TextStyle style;
    style.font = scene.width < 300 ? 3 : n == 5 && scene.width >= 400 ? 1 : 0;
    style.align = fui::TextAlign::Center;
    style.bold = true;
    const int side = target.lineHeight(style.font) + 4;
    const auto glyphBox = fui::makeRect(center.x - side / 2, center.y - side / 2, side, side);
    const char glyph[] = {char('A' + seed % 26), '\0'};
    const int outline = scene.width < 300 ? 1 : 2;
    style.color = fui::Color::White;
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx)
        if (dx || dy)
          target.text(fui::makeRect(glyphBox.x + dx * outline, glyphBox.y + dy * outline, side, side), glyph, style);
    style.color = fui::Color::Black;
    target.text(glyphBox, glyph, style);
  }
  if (phase == Phase::Impact) {
    static constexpr int RAYS[][4] = {{-28, -18, -38, -25}, {18, -25, 25, -36}, {28, 4, 42, 6}, {14, 24, 21, 36}};
    for (const auto& ray : RAYS) {
      const auto from = point(toX + ray[0], toY + ray[1]), to = point(toX + ray[2], toY + ray[3]);
      target.line(from, to, 5, fui::Paint::solid(fui::Color::White));
      target.line(from, to, 2, fui::Paint::solid(fui::Color::Black));
    }
  }
  target.setClipRect(previous);
}
}  // namespace questreturn
