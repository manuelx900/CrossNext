#include <FreeInkUIDisplayTarget.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <vector>

#include "../../src/apps_local/pet/PetWidgets.h"
#include "../../src/apps_local/ui/SdBitmapPack.h"

namespace fui = freeink::ui;
struct Probe : fui::DrawTarget {
  fui::Color color = fui::Color::Transparent;
  unsigned fills = 0;
  fui::Rect clip{1, 2, 80, 70};
  fui::Rect clipRect() const override { return clip; }
  bool setClipRect(fui::Rect r) override {
    clip = r;
    return true;
  }
  fui::Size measureText(fui::FontId, const char*, fui::TextStyle) const override { return {20, 10}; }
  int16_t lineHeight(fui::FontId) const override { return 10; }
  void fill(fui::Rect, fui::Paint p, uint8_t = 0, uint8_t = fui::CornersAll) override {
    color = p.color;
    ++fills;
  }
  void stroke(fui::Rect, fui::Paint, uint8_t, uint8_t = 0, uint8_t = fui::CornersAll) override {}
  void line(fui::Point, fui::Point, uint8_t, fui::Paint) override {}
  void triangle(fui::Point, fui::Point, fui::Point, fui::Paint) override {}
  void text(fui::Rect, const char*, fui::TextStyle) override {}
  void bitmap(fui::Rect, fui::BitmapRef, fui::BitmapMode, fui::Paint, fui::Rotation) override {}
};
struct Callback {
  unsigned calls = 0, masks = 0;
  std::array<unsigned, 4> order{};
  unsigned items = 0;
  static bool draw(void* context, fui::DrawTarget& target, fui::Rect r, uint32_t id, bool inverse) {
    auto& self = *static_cast<Callback*>(context);
    ++self.calls;
    self.masks += inverse;
    if (inverse && id >= 2 && id < 482 && self.items < self.order.size()) self.order[self.items++] = (id - 2) / 2;
    target.fill(r, fui::Paint::dither(inverse ? fui::Color::White : fui::Color::DarkGray));
    return true;
  }
};
void depthChecks() {
  using namespace petui::habitatlayout;
  const auto overlaps = [](fui::Rect a, fui::Rect b) {
    return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
  };
  unsigned available = 0, pairs = 0;
  pet::Profile p;
  p.created = true;
  for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) {
    if (!pet::objectAvailable(id)) continue;
    ++available;
    const auto box = item(id);
    assert(box.x >= 0 && box.y >= 0 && box.right() <= 432 && box.bottom() <= 216);
    if (mounted(id)) assert(!overlaps(box, rect(338, 10, 80, 68)));
    if (!mounted(id) && anchor(id) != Anchor::Toy) continue;
    for (unsigned species = 0; species < 3; ++species)
      for (unsigned stage = 0; stage < 5; ++stage) {
        p.species = pet::Species(species);
        p.readingSeconds = pet::stageStart(pet::Stage(stage));
        assert(!overlaps(box, petui::habitatlayout::pet(p)));
      }
  }
  for (unsigned decoration = 0; decoration < pet::OBJECT_COUNT; ++decoration)
    for (unsigned environment = 0; environment < pet::OBJECT_COUNT; ++environment) {
      if (!sharedWall(decoration, environment)) continue;
      ++pairs;
      for (const auto scene : {rect(0, 0, 432, 216), rect(6, 251, 426, 213), rect(10, 20, 216, 108)}) {
        const auto wallTop = scene.width == 426 ? 279 : -1;
        const auto a = placedItem(scene, decoration, wallTop, true);
        const auto b = placedItem(scene, environment, wallTop, true);
        assert(!a.empty() && !b.empty() && !overlaps(a, b));
        assert(a.right() < scene.x + 163 * scene.width / 432);
        assert(b.right() < scene.x + 163 * scene.width / 432);
        for (const auto id : {decoration, environment}) {
          const auto fitted = placedItem(scene, id, wallTop, true);
          const auto& source = petui::OBJECT_ART[id];
          // One source-pixel rounding at each fit/scale boundary.
          assert(std::abs(fitted.width * source.height - fitted.height * source.width) <
                 2 * (source.width + source.height));
        }
      }
    }
  assert(available == pet::CATALOG_COUNT && pairs == 14 * 26);
  std::printf(
      "PET depth: %u items, 15 pet silhouettes, %u wall pairs, window clearance and foreground toy clearance PASS\n",
      available, pairs);
}
int main(int argc, char** argv) {
  depthChecks();
  Probe probe;
  petui::DecorationInk ink(probe);
  for (auto color :
       {fui::Color::White, fui::Color::LightGray, fui::Color::DarkGray, fui::Color::Black, fui::Color::Transparent}) {
    for (auto paint : {fui::Paint::solid(color), fui::Paint::dither(color)}) {
      ink.fill({0, 0, 4, 4}, paint);
      assert(probe.color == (color == fui::Color::DarkGray ? fui::Color::Black : color));
    }
  }
  ink.setClipRect({3, 4, 50, 60});
  assert(probe.clip.x == 3 && ink.clipRect().height == 60);
  // Mounted objects clear the title and every bed; memories keep the same room anchors.
  struct Room {
    fui::Rect scene;
    int wallTop;
  };
  for (const Room room : {Room{{0, 300, 480, 240}, 270}, Room{{20, 260, 400, 200}, 230}, Room{{6, 251, 426, 213}, 279},
                          Room{{6, 249, 432, 216}, 254}, Room{{10, 20, 216, 108}, -1}}) {
    const auto scene = room.scene;
    const int wallTop = room.wallTop;
    unsigned hanging = 0, mounted = 0;
    for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) {
      if (!pet::objectAvailable(id)) continue;
      const auto original = petui::habitatlayout::scale(scene, petui::habitatlayout::item(id));
      const auto placed = petui::habitatlayout::placedItem(scene, id, wallTop);
      hanging += petui::OBJECT_ART[id].anchor == 3;
      if (petui::habitatlayout::mounted(id)) {
        ++mounted;
        const int mountingY = wallTop < 0 ? scene.y : wallTop;
        assert(placed.y > mountingY);
        assert(placed.width <= original.width && placed.height <= original.height);
        assert(std::abs(placed.width * original.height - original.width * placed.height) < original.height);
        for (unsigned bed = 0; bed < pet::OBJECT_COUNT; ++bed)
          if (pet::objectSlot(bed) == 0)
            assert(placed.bottom() < petui::habitatlayout::scale(scene, petui::habitatlayout::item(bed)).y);
      } else {
        assert(placed.x == original.x && placed.y == original.y && placed.width == original.width &&
               placed.height == original.height);
      }
      if (wallTop < 0) assert(placed.y == original.y);
    }
    assert(hanging == 4 && mounted == 40);
  }
  const auto noWall = petui::habitatlayout::placedItem({0, 0, 432, 216}, 120, 190);
  assert(noWall.empty());
  pet::Profile profile;
  petui::Model model;
  model.profile = &profile;
  Callback callback;
  model.habitat = {&callback, Callback::draw};
  model.art = {&callback, Callback::draw};
  fui::DeviceContext device;
  device.width = 480;
  device.height = 800;
  toybox::Interactions interactions;
  toybox::Frame frame(probe, device, {}, interactions);
  toybox::Screen screen(frame);
  for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) {
    callback = {};
    if (!pet::objectAvailable(id)) {
      assert(!petui::habitatPair(model, probe, {0, 0, 20, 20}, 2 + id * 2));
      petui::widget::object(screen, model, {0, 0, 20, 20}, id);
      assert(callback.calls == 0);
      continue;
    }
    assert(petui::habitatPair(model, probe, {0, 0, 20, 20}, 2 + id * 2));
    assert(callback.calls == 2 && callback.masks == 1 && probe.color == fui::Color::Black);
    callback = {};
    petui::widget::object(screen, model, {0, 0, 20, 20}, id);
    assert(callback.calls == 1 && probe.color == fui::Color::Black);
    callback = {};
    petui::widget::object(screen, model, {0, 0, 20, 20}, id, true);
    assert(callback.calls == 1 && probe.color == fui::Color::White);
  }
  for (unsigned scene = 0; scene < 39; ++scene) {
    callback = {};
    assert(petui::habitatPair(model, probe, {0, 0, 20, 20}, 482 + scene * 2));
    assert(callback.calls == 2 && callback.masks == 1 && probe.color == fui::Color::DarkGray);
  }
  for (unsigned environment = 0; environment < pet::OBJECT_COUNT; ++environment) {
    if (pet::objectSlot(environment) != 3) continue;
    callback = {};
    profile.equipped[0] = 0;
    profile.equipped[1] = profile.equipped[2] = 255;
    profile.equipped[3] = uint8_t(environment);
    petui::habitatFigures(screen, model, {0, 300, 432, 216}, false);
    assert(callback.items == 2);
    const bool ground = petui::OBJECT_ART[environment].anchor == 5;
    assert(callback.order[0] == (ground ? 0 : environment));
    assert(callback.order[1] == (ground ? environment : 0));
  }
  // The foreground stays in front of the bed lip when the companion is asleep.
  callback = {};
  probe.clip = {0, 0, 480, 800};
  profile.created = true;
  profile.readingSeconds = pet::stageStart(pet::Stage::Adult);
  profile.resting = true;
  profile.equipped[0] = 47;
  profile.equipped[3] = 234;
  petui::habitatFigures(screen, model, {0, 300, 432, 216}, true);
  assert(callback.items == 3 && callback.order[0] == 47 && callback.order[1] == 47 && callback.order[2] == 234);
  model.habitat = {};
  for (unsigned id = 0; id < 12; ++id) {
    if (!pet::objectAvailable(id)) continue;
    callback = {};
    petui::widget::object(screen, model, {0, 0, 20, 20}, id);
    assert(callback.calls == 1 && probe.color == fui::Color::Black);
  }
  // Render real packed ink: every pixel is either preserved or becomes black.
  sdart::Pack pack;
  assert(pack.open("/apps/pet/habitat.bin", 560, 65536));
  size_t darkened = 0;
  std::vector<uint8_t> before(80 * 96 / 8), after(before.size());
  for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) {
    std::fill(before.begin(), before.end(), 255);
    after = before;
    fui::DisplayTarget a(before.data(), 80, 96, 10), b(after.data(), 80, 96, 10);
    petui::DecorationInk stronger(b);
    pack.draw(a, {0, 0, 80, 96}, 3 + id * 2);
    const auto reads = sdartReadCalls, seeks = sdartSeekCalls;
    pack.draw(stronger, {0, 0, 80, 96}, 3 + id * 2);
    assert(pack.ready() && reads == sdartReadCalls && seeks == sdartSeekCalls);
    for (size_t i = 0; i < before.size(); ++i) {
      assert((after[i] | before[i]) == before[i]);
      darkened += after[i] != before[i];
    }
    if (argc > 1 && id == 0) {
      for (unsigned phase = 0; phase < 2; ++phase) {
        char name[512];
        std::snprintf(name, sizeof(name), "%s/%s.pbm", argv[1], phase ? "after" : "before");
        auto* file = std::fopen(name, "wb");
        assert(file);
        std::fprintf(file, "P4\n80 96\n");
        for (auto byte : phase ? after : before) std::fputc(byte ^ 255, file);
        std::fclose(file);
      }
    }
  }
  assert(darkened > 0);
  model.reactionKind = petreact::Kind::Petting;
  for (const auto phase : {petreact::Phase::First, petreact::Phase::Second}) {
    model.reactionPhase = phase;
    callback = {};
    petui::roomeffects::paint(screen, model, {180, 380, 120, 140});
    assert(callback.calls == 1 && callback.masks == 0 && probe.color == fui::Color::Black);
  }
  sdart::Pack graphics;
  assert(graphics.open("/apps/pet/graphics.bin", petui::ART_COUNT, 65536));
  std::fill(before.begin(), before.end(), 255);
  after = before;
  fui::DisplayTarget handBefore(before.data(), 80, 96, 10), handAfter(after.data(), 80, 96, 10);
  petui::DecorationInk outlined(handAfter);
  graphics.draw(handBefore, {0, 0, 68, 48}, petui::PETTING_HAND_ART);
  assert(graphics.ready());
  const auto handReads = sdartReadCalls, handSeeks = sdartSeekCalls;
  graphics.draw(outlined, {0, 0, 68, 48}, petui::PETTING_HAND_ART);
  assert(graphics.ready());
  assert(handReads == sdartReadCalls && handSeeks == sdartSeekCalls);
  bool strongerHand = false;
  for (size_t i = 0; i < before.size(); ++i) {
    assert((after[i] | before[i]) == before[i]);
    strongerHand = strongerHand || after[i] != before[i];
  }
  assert(strongerHand);
  profile.resting = false;
  for (unsigned species = 0; species < 3; ++species)
    for (unsigned stage = 1; stage < 5; ++stage)
      for (const auto area :
           {petui::habitatlayout::rect(0, 300, 480, 240), petui::habitatlayout::rect(20, 260, 400, 200)}) {
        profile.species = pet::Species(species);
        profile.readingSeconds = pet::stageStart(pet::Stage(stage));
        const auto actor = petui::habitatlayout::scale(area, petui::habitatlayout::pet(profile));
        const auto badge = petmood::roomBounds(area, actor.inset({int16_t(actor.height / 8), 0, 0, 0}), nullptr, 0);
        const auto first = petui::roomeffects::pettingHandBounds(actor, false);
        const auto second = petui::roomeffects::pettingHandBounds(actor, true);
        assert(first.width == 68 && first.height == 48 && second.x == first.x + 8 && second.y == first.y);
        assert(first.x >= area.x && first.y >= area.y && second.right() <= area.right());
        assert(petmood::overlap(first, badge) == 0 && petmood::overlap(second, badge) == 0);
      }
  if (argc > 1) {
    for (const bool strong : {false, true}) {
      char path[512];
      std::snprintf(path, sizeof(path), "%s/hand-%s.pbm", argv[1], strong ? "black" : "before");
      auto* file = std::fopen(path, "wb");
      assert(file);
      std::fprintf(file, "P4\n80 96\n");
      for (auto byte : strong ? after : before) std::fputc(byte ^ 255, file);
      std::fclose(file);
    }
  }
  std::puts(
      "PET petting hand: solid black dark contours, light fills retained, both room frames and warm-cache reads PASS");
  std::puts(
      "PET placement: 223 current anchors, 4 ceiling mounts, portrait/landscape/memory, "
      "aspect ratio, floor baselines and scenery draw order PASS");
  std::puts(
      "PET decoration ink: 240 objects, white masks/inversion, 39 untouched pet poses, legacy fallback and cached real "
      "pixels PASS");
}
