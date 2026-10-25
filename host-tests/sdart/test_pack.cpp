#include <FreeInkUIDisplayTarget.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../../assets_local/quest/encoded/QuestArt.h"
#include "../../assets_local/quest/encoded/QuestAuraArt.h"
#include "../../assets_local/quest/encoded/QuestFigures.h"
#include "../../assets_local/quest/encoded/QuestOutfitIcons.h"
#include "../../src/apps_local/quest/QuestAssets.h"

namespace fui = freeink::ui;
int main(int argc, char** argv) {
  if (argc < 2) return 2;
  auto* input = std::fopen(argv[1], "rb");
  char magic[8]{};
  if (!input) return 2;
  const bool modern = std::fread(magic, 1, 8, input) == 8 && magic[7] == '2';
  std::fclose(input);
  for (int memoryMode = 0; memoryMode < 3; ++memoryMode) {
    sdartMemory::available = memoryMode != 1;
    sdartMemory::denyAllocation = memoryMode == 2;
    sdartReadCalls = sdartReadBytes = sdartSeekCalls = 0;
    sdart::Pack pack;
    if (!pack.open(argv[1], questassets::COUNT)) return 3;
    std::printf("OPEN mode=%d reads=%zu seeks=%zu bytes=%zu cache=%zu\n", memoryMode, sdartReadCalls, sdartSeekCalls,
                sdartReadBytes, sdartMemory::liveBytes);
    if (sdartMemory::liveBytes != (memoryMode == 0 ? questassets::COUNT * (modern ? 16 : 12) + 4096 : 0)) return 11;
    struct Group {
      uint32_t start;
      const flashbitmap::Sprite* images;
      int count;
    };
    const Group groups[] = {{0, questfigures::figures, 200}, {200, questfigures::figuresMasks, 200},
                            {400, questart::pets, 50},       {450, questart::petsMasks, 50},
                            {500, questart::petIcons, 50},   {550, questart::petIconMasks, 50},
                            {600, questauras::motifs, 50},   {650, questauras::motifsMasks, 50},
                            {700, questoutfits::icons, 100}, {800, questoutfits::iconsMasks, 100}};
    int checked = 0;
    for (const auto& group : groups)
      for (int i = 0; i < group.count; ++i) {
        const auto& sprite = group.images[i];
        const auto info = pack.image(group.start + i);
        if (info.width != sprite.width || info.height != sprite.height) return 4;
        for (int scale : {1, 2}) {
          const int width = 280 / scale, height = 320 / scale;
          std::vector<uint8_t> oldPixels((width + 7) / 8 * height, 255), newPixels(oldPixels);
          fui::DisplayTarget oldTarget(oldPixels.data(), width, height, (width + 7) / 8);
          fui::DisplayTarget newTarget(newPixels.data(), width, height, (width + 7) / 8);
          auto area = fui::makeRect(0, 0, width, height);
          flashbitmap::draw(oldTarget, area, sprite);
          pack.draw(newTarget, area, group.start + i);
          if (!pack.ready() || oldPixels != newPixels) {
            std::printf("Sprite differs: %u scale %d\n", group.start + i, scale);
            return 5;
          }
        }
        ++checked;
      }
    sdartReadFailure = true;
    std::vector<uint8_t> pixels(280 * 320 / 8, 255);
    fui::DisplayTarget target(pixels.data(), 280, 320, 35);
    pack.draw(target, fui::makeRect(0, 0, 280, 320), 0);
    if (pack.ready()) return 6;
    if (sdartMemory::liveBytes) return 12;
    sdartReadFailure = false;
    if (!pack.open(argv[1], questassets::COUNT)) return 7;
    pack.close();
    if (pack.ready()) return 8;
    if (sdartMemory::liveBytes) return 13;
    if (pack.open(argv[1], 899)) return 9;
    for (int i = 2; i < argc; ++i) {
      if (pack.open(argv[i], questassets::COUNT)) {
        // A corrupt v2 payload is checked lazily, before its first pixels are drawn.
        pack.draw(target, fui::makeRect(0, 0, 280, 320), questassets::COUNT - 1);
        if (pack.ready()) return 10;
      }
      if (pack.ready() || sdartMemory::liveBytes) return 14;
    }
    for (size_t failure : {2, 5}) {
      sdartReadCalls = 0;
      sdartFailReadCall = failure;
      if (pack.open(argv[1], questassets::COUNT) || pack.ready() || sdartMemory::liveBytes) return 15;
      sdartFailReadCall = 0;
    }
    if (!pack.open(argv[1], questassets::COUNT)) return 16;
    pack.image(questassets::COUNT);
    if (pack.ready() || sdartMemory::liveBytes) return 17;
    std::printf(
        "PASS: %d sprites identical at two scales; read failure, reopen, close, wrong count and %d invalid packs "
        "rejected\n",
        checked, argc - 2);
  }
}
