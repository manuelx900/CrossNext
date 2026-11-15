#pragma once

#include "PetDecorationInk.h"
#include "PetMoodMarks.h"
#include "PetSymbols.h"

namespace petui::roomeffects {
inline constexpr auto ink = fui::Paint::solid(fui::Color::Black);
inline constexpr auto paper = fui::Paint::solid(fui::Color::White);
inline void star(fui::DrawTarget& target, int x, int y, int size) {
  target.line({int16_t(x - size), int16_t(y)}, {int16_t(x + size), int16_t(y)}, 1, ink);
  target.line({int16_t(x), int16_t(y - size)}, {int16_t(x), int16_t(y + size)}, 1, ink);
}
enum class ConfettiLayer : uint8_t { Behind, Front };
enum class ConfettiShape : uint8_t { Strip, Diamond, Paper, Ribbon };
struct Particle {
  int8_t x, depth;
  uint8_t height;
  ConfettiShape shape;
};
inline constexpr Particle PARTICLES[] = {{-70, -70, 44, ConfettiShape::Strip},   {74, -70, 42, ConfettiShape::Paper},
                                         {-64, -62, 88, ConfettiShape::Diamond}, {66, -62, 86, ConfettiShape::Diamond},
                                         {-38, -58, 126, ConfettiShape::Paper},  {0, -56, 142, ConfettiShape::Ribbon},
                                         {40, -56, 126, ConfettiShape::Strip},   {-88, -30, 28, ConfettiShape::Ribbon},
                                         {88, -30, 28, ConfettiShape::Strip},    {-86, -15, 72, ConfettiShape::Diamond},
                                         {86, -15, 72, ConfettiShape::Strip},    {-58, -8, 112, ConfettiShape::Paper},
                                         {58, -8, 112, ConfettiShape::Ribbon},   {-66, 20, 80, ConfettiShape::Diamond},
                                         {66, 20, 80, ConfettiShape::Paper},     {-44, 28, 120, ConfettiShape::Ribbon},
                                         {44, 28, 120, ConfettiShape::Strip},    {-78, 48, 35, ConfettiShape::Diamond},
                                         {78, 48, 35, ConfettiShape::Paper},     {-42, 62, 30, ConfettiShape::Ribbon},
                                         {42, 62, 30, ConfettiShape::Strip},     {-20, 78, 15, ConfettiShape::Diamond},
                                         {24, 84, 12, ConfettiShape::Paper},     {2, 90, 5, ConfettiShape::Ribbon}};
struct Projection {
  int x, y, groundY, size, lift;
};
inline Projection project(fui::Rect actor, const Particle& p, bool falling) {
  const int perspective = 100 + p.depth / 2;
  Projection result;
  result.x = actor.x + actor.width / 2 + p.x * int(actor.width) * (perspective + (falling ? 8 : 0)) / 10000;
  result.groundY = actor.bottom() + p.depth * std::min(120, int(actor.height)) / 400;
  result.lift = std::max(0, int(p.height) - (falling ? 28 : 0));
  result.y = result.groundY - result.lift * int(actor.height) * perspective / 10000;
  result.size = 4 + (p.depth + 70) / 26;
  return result;
}
inline bool celebrating(const Model& m) {
  return (m.reactionKind == petreact::Kind::Game && m.gameResult.outcome != GameOutcome::CareNeeded) ||
         (m.reactionKind == petreact::Kind::Growth &&
          (m.reactionPhase == petreact::Phase::Reveal || m.reactionPhase == petreact::Phase::Result));
}
inline void confetti(fui::DrawTarget& target, fui::Rect actor, bool falling, ConfettiLayer layer) {
  for (const auto& particle : PARTICLES) {
    if ((particle.depth < 0) != (layer == ConfettiLayer::Behind)) continue;
    const auto p = project(actor, particle, falling);
    const int x = p.x, y = p.y, size = p.size, vertical = p.lift ? size : 2;
    const auto shade = fui::Paint::dither(particle.depth < 0 ? fui::Color::DarkGray : fui::Color::Black);
    const auto point = [](int px, int py) { return fui::Point{int16_t(px), int16_t(py)}; };
    switch (particle.shape) {
      case ConfettiShape::Strip:
        target.line(point(x - size / 3, y - vertical), point(x + size / 3, y + vertical), particle.depth < 0 ? 2 : 4,
                    shade);
        break;
      case ConfettiShape::Diamond:
        target.triangle(point(x, y - vertical), point(x + size, y), point(x, y + vertical), shade);
        target.triangle(point(x, y - vertical), point(x, y + vertical), point(x - size, y), shade);
        break;
      case ConfettiShape::Paper: {
        const auto piece = fui::makeRect(x - size, y - vertical / 3, size * 2, std::max(2, vertical * 2 / 3));
        target.fill(piece, paper, 1);
        target.stroke(piece, shade, particle.depth < 0 ? 1 : 2, 1);
        break;
      }
      case ConfettiShape::Ribbon:
        target.line(point(x - size / 2, y - vertical), point(x + size / 2, y - vertical / 3), 2, shade);
        target.line(point(x + size / 2, y - vertical / 3), point(x - size / 2, y + vertical / 3), 2, shade);
        target.line(point(x - size / 2, y + vertical / 3), point(x + size / 2, y + vertical), 2, shade);
        break;
    }
  }
}
inline void behind(toybox::Screen& screen, const Model& m, fui::Rect actor) {
  if (!celebrating(m)) return;
  const bool falling = m.reactionPhase == petreact::Phase::Second || m.reactionPhase == petreact::Phase::Result;
  for (const auto& particle : PARTICLES) {
    const auto p = project(actor, particle, falling);
    const auto shadow = fui::makeRect(p.x + 3 - p.size / 2, p.groundY + 2, p.size + 3, 2 + int(particle.depth > 0));
    screen.target().fill(shadow, fui::Paint::dither(fui::Color::LightGray), 1);
  }
  confetti(screen.target(), actor, falling, ConfettiLayer::Behind);
}

inline void hearts(toybox::Screen& screen, fui::Rect actor, bool rising) {
  for (unsigned i = 0; i < 3; ++i) {
    const int side = 14 + int(i % 2) * 4;
    const int x = actor.x + (int(i) * 55 - 4) * actor.width / 100;
    const int y = actor.y - 20 - int(i % 2) * 12 - (rising ? 10 : 0);
    symbols::draw(screen, fui::makeRect(x, y, side, side), symbols::Kind::Health);
  }
}
inline fui::Rect pettingHandBounds(fui::Rect actor, bool second) {
  constexpr int width = 68, height = 48, travel = 8, gap = 4;
  const int right =
      std::min(actor.x + actor.width / 5 + 24 + width / 2, actor.right() - petmood::ROOM_MAX_SIDE - gap - travel);
  return fui::makeRect(right - width + (second ? travel : 0), actor.y - 24, width, height);
}
inline void paint(toybox::Screen& screen, const Model& m, fui::Rect actor) {
  const bool second = m.reactionPhase == petreact::Phase::Second;
  const bool result = m.reactionPhase == petreact::Phase::Result;
  switch (m.reactionKind) {
    case petreact::Kind::None:
      return;
    case petreact::Kind::Growth:
      if (m.reactionPhase == petreact::Phase::Reveal || result)
        confetti(screen.target(), actor, result, ConfettiLayer::Front);
      return;
    case petreact::Kind::Game:
      if (m.gameResult.outcome != GameOutcome::CareNeeded)
        confetti(screen.target(), actor, second || result, ConfettiLayer::Front);
      return;
    case petreact::Kind::Petting: {
      hearts(screen, actor, second || result);
      DecorationInk handInk(screen.target());
      if (!result && m.art.draw)
        m.art.draw(m.art.context, handInk, pettingHandBounds(actor, second), PETTING_HAND_ART, false);
      return;
    }
    case petreact::Kind::Healing:
      hearts(screen, actor, second || result);
      star(screen.target(), actor.right() + 8, actor.y + actor.height / 3, 5);
      star(screen.target(), actor.x - 8, actor.bottom() - 16, 4);
      return;
    case petreact::Kind::Cleaning:
      if (!result && m.reactionArt == petreact::Art::None) {
        static constexpr uint8_t points[][2] = {{4, 76}, {23, 85}, {47, 68}, {72, 80}, {94, 72}, {12, 96}, {65, 94}};
        const unsigned count = second ? 4 : 7;
        for (unsigned i = 0; i < count; ++i) {
          const int side = 10 + int(i % 3) * 3;
          const auto bubble =
              fui::makeRect(actor.x - 4 + points[i][0] * actor.width / 100,
                            actor.y + points[i][1] * actor.height / 100 - (second ? 25 : 0), side, side);
          screen.target().fill(bubble, paper, uint8_t(side / 2));
          screen.target().stroke(bubble, fui::Paint::dither(fui::Color::DarkGray), 1, uint8_t(side / 2));
        }
      }
      if (second || result) {
        star(screen.target(), actor.x - 9, actor.y + actor.height / 2, 5);
        star(screen.target(), actor.right() + 7, actor.y + actor.height / 4, 6);
      }
      return;
  }
}
}  // namespace petui::roomeffects
