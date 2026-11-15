#pragma once
#include <FreeInkUI.h>

#include <algorithm>

#include "QuestAssets.h"
#include "QuestCore.h"
#include "QuestExpansionArt.h"

namespace questavatar {
namespace fui = freeink::ui;
enum class Framing : uint8_t { Contain, Tall, Cover };
enum class Cast : uint8_t { Equipped, HeroOnly };
inline fui::Rect sceneRect(fui::Rect area) {
  const int width = std::min(int(area.width), int(area.height) * 3 / 2);
  const int height = width * 2 / 3;
  return fui::makeRect(area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height);
}
inline fui::Rect groundedSceneRect(fui::Rect area) {
  auto scene = sceneRect(area);
  if (area.height * 3 > area.width * 2) scene.y = area.bottom() - area.height / 16 - scene.height * 300 / 320;
  return scene;
}
inline void paint(sdart::Pack& assets, fui::DrawTarget& target, fui::Rect rect, uint32_t ink, uint32_t mask,
                  fui::BitmapMode mode = fui::BitmapMode::Contain) {
  assets.draw(target, rect, mask, mode, fui::Color::White);
  assets.draw(target, rect, ink, mode);
}
inline void background(fui::DrawTarget& target, fui::Rect area, sdart::Pack& assets, unsigned index) {
  if (index >= quest::kBackgroundCount || area.empty()) return;
  const int middle = area.x + area.width / 2;
  assets.draw(target, fui::makeRect(area.x, area.y, middle - area.x, area.height), questassets::BACKGROUND_LEFT + index,
              fui::BitmapMode::Stretch);
  assets.draw(target, fui::makeRect(middle, area.y, area.right() - middle, area.height),
              questassets::BACKGROUND_RIGHT + index, fui::BitmapMode::Stretch);
}
inline void backgroundTiles(fui::DrawTarget& target, fui::Rect area, sdart::Pack& assets, uint32_t first) {
  const int middle = area.x + area.width / 2;
  assets.draw(target, fui::makeRect(area.x, area.y, middle - area.x, area.height), first, fui::BitmapMode::Stretch);
  assets.draw(target, fui::makeRect(middle, area.y, area.right() - middle, area.height), first + 1,
              fui::BitmapMode::Stretch);
}
inline void backgroundCover(fui::DrawTarget& target, fui::Rect area, sdart::Pack& assets, uint32_t first, bool tiles) {
  if (area.empty()) return;
  const auto previous = target.clipRect();
  const auto clipped = fui::makeRect(std::max(area.x, previous.x), std::max(area.y, previous.y),
                                     std::min(area.right(), previous.right()) - std::max(area.x, previous.x),
                                     std::min(area.bottom(), previous.bottom()) - std::max(area.y, previous.y));
  if (clipped.empty() || !target.setClipRect(clipped)) return;
  const int width = std::max(int(area.width), (int(area.height) * 3 + 1) / 2);
  const int height = (width * 2 + 2) / 3;
  const auto camera =
      fui::makeRect(area.x + (area.width - width) / 2, area.y + (area.height - height) / 2, width, height);
  if (tiles)
    backgroundTiles(target, camera, assets, first);
  else
    background(target, camera, assets, first);
  target.setClipRect(previous);
}
inline fui::Rect storyBackground(fui::DrawTarget& target, fui::Rect area, sdart::Pack& assets, uint32_t first,
                                 Framing framing = Framing::Contain) {
  auto scene = sceneRect(area);
  if (scene.empty()) return scene;
  if (framing == Framing::Cover) {
    backgroundCover(target, area, assets, first, true);
    return scene;
  }
  const bool tall = framing == Framing::Tall && area.height * 3 > area.width * 2;
  if (tall) {
    const auto previousClip = target.clipRect();
    const auto clipped =
        fui::makeRect(std::max(area.x, previousClip.x), std::max(area.y, previousClip.y),
                      std::min(area.right(), previousClip.right()) - std::max(area.x, previousClip.x),
                      std::min(area.bottom(), previousClip.bottom()) - std::max(area.y, previousClip.y));
    if (target.setClipRect(clipped)) {
      const int width = area.height * 3 / 2;
      backgroundTiles(target, fui::makeRect(area.x + (area.width - width) / 2, area.y, width, area.height), assets,
                      first);
      target.setClipRect(previousClip);
      scene = groundedSceneRect(area);
    } else {
      backgroundTiles(target, scene, assets, first);
    }
  } else {
    backgroundTiles(target, scene, assets, first);
  }
  return scene;
}
inline void draw(fui::DrawTarget& target, fui::Rect area, const quest::Profile& profile, sdart::Pack& assets,
                 int preview = -1, uint32_t storyTiles = 0, Framing framing = Framing::Contain,
                 sdart::Pack* rewards = nullptr, Cast cast = Cast::Equipped, sdart::Pack* outfits = nullptr,
                 sdart::Pack* celebration = nullptr, uint32_t celebrationTiles = 0) {
  target.fill(area, fui::Paint::solid(fui::Color::White));
  auto scene = sceneRect(area);
  if (scene.empty()) return;
  const auto equipped = [&](quest::Slot slot) {
    return quest::visibleItem(preview) && quest::slot(preview) == slot ? preview : int(profile.equipped[int(slot)]);
  };
  const int selected = equipped(quest::Slot::Body);
  const int body = quest::visibleItem(selected) && quest::slot(selected) == quest::Slot::Body ? selected : 0;
  const int backdrop = equipped(quest::Slot::Background) - quest::first(quest::Slot::Background);
  const auto heroSprite = questextra::figure(assets, rewards, body, profile.gender, outfits);
  const auto figureArt = heroSprite.pack ? heroSprite.pack->image(heroSprite.ink) : sdart::Image{};
  if (!assets.ready() || !figureArt.height) return;
  const auto petSprite =
      cast == Cast::HeroOnly ? questextra::Sprite{} : questextra::pet(assets, rewards, equipped(quest::Slot::Pet));
  const auto petArt = petSprite.pack ? petSprite.pack->image(petSprite.ink) : sdart::Image{};
  const bool hasPet = petArt.height != 0;
  if (!assets.ready()) return;
  // A fixed 480x320 scene camera keeps equipment changes from zooming the hero.
  constexpr int BASELINE = 300, SEPARATION = 14;
  const bool tall = storyTiles && framing == Framing::Tall && area.height * 3 > area.width * 2;
  const int HERO_HEIGHT = celebration ? 150 : tall ? 214 : storyTiles ? 190 : 244;
  const int heroWidth = int(figureArt.width) * HERO_HEIGHT / figureArt.height;
  const int petWidth = int(petArt.width) * HERO_HEIGHT / 244, petHeight = int(petArt.height) * HERO_HEIGHT / 244;
  const int groupWidth = heroWidth + (hasPet ? SEPARATION + petWidth : 0);
  const int left = (480 - groupWidth) / 2;
  const auto rect = [&](int x, int y, int width, int height) {
    const int px = scene.x + x * scene.width / 480, py = scene.y + y * scene.height / 320;
    return fui::makeRect(px, py, scene.x + (x + width) * scene.width / 480 - px,
                         scene.y + (y + height) * scene.height / 320 - py);
  };
  if (celebration)
    backgroundTiles(target, scene, *celebration, celebrationTiles);
  else if (storyTiles)
    scene = storyBackground(target, area, assets, storyTiles, framing);
  else if (backdrop >= 0 && backdrop < quest::kBackgroundCount) {
    if (framing == Framing::Cover)
      backgroundCover(target, area, assets, backdrop, false);
    else
      background(target, scene, assets, backdrop);
  } else if (equipped(quest::Slot::Background) == quest::kKingBackground && rewards) {
    if (framing == Framing::Cover)
      backgroundCover(target, area, *rewards, questassets::KING_BACKGROUND, true);
    else
      questextra::tiles(target, scene, *rewards, questassets::KING_BACKGROUND);
  }
  if (!celebration && !storyTiles && cast == Cast::Equipped) {
    const int item = quest::availableItem(preview) && quest::slot(preview) == quest::Slot::Decoration
                         ? preview
                         : quest::activeDecoration(profile);
    const int place = quest::decorationPlace(item);
    if (place >= 0) {
      const auto heroBox = fui::makeRect(left, BASELINE - HERO_HEIGHT, heroWidth, HERO_HEIGHT);
      const auto petBox = hasPet
                              ? fui::makeRect(left + heroWidth + SEPARATION, BASELINE - petHeight, petWidth, petHeight)
                              : fui::Rect{};
      const auto decoration = questextra::decorationBox(item, place, heroBox, petBox);
      questextra::paint(target, rect(decoration.x, decoration.y, decoration.width, decoration.height),
                        questextra::decoration(assets, rewards, item));
    }
  }
  questextra::paint(target, rect(left, BASELINE - HERO_HEIGHT, heroWidth, HERO_HEIGHT), heroSprite);
  if (hasPet)
    questextra::paint(target, rect(left + heroWidth + SEPARATION, BASELINE - petHeight, petWidth, petHeight),
                      petSprite);
}
}  // namespace questavatar
