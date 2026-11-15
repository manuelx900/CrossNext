#include <array>
#include <cassert>
#include <cstdio>

#include "../../src/apps_local/pet/PetCatalogView.h"
#include "../../src/apps_local/pet/PetHabitat.h"

namespace fui = freeink::ui;
static constexpr unsigned RETIRED[] = {5, 88, 89, 90, 91, 92, 93, 94, 95, 96, 99, 100, 101, 102, 103, 104, 105};
void storedOwn(pet::Profile& p, unsigned id) {
  if (id < 16)
    p.owned |= uint16_t(1u << id);
  else
    p.ownedExtra[(id - 16) / 8] |= uint8_t(1u << ((id - 16) % 8));
}
int main() {
  static_assert(pet::CATALOG_COUNT == 223);
  static_assert(pet::STATE_BYTES == 640 && pet::EXTRA_OWNED_BYTES == 28);
  pet::Profile p;
  p.created = true;
  p.readingSeconds = 72000;
  for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) {
    bool retired = false;
    for (unsigned old : RETIRED) retired |= id == old;
    assert(pet::objectAvailable(id) == !retired);
    pet::own(p, id);
    assert(pet::owns(p, id) == !retired);
  }
  assert(pet::ownedCount(p) == 223 && pet::collectionComplete(p));
  for (unsigned slot = 0; slot < pet::SLOT_COUNT; ++slot) {
    const unsigned expected = slot == 1 ? 43 : 60;
    assert(petui::catalogCount(p, slot, petui::CatalogFilter::Owned) == expected);
    assert(petui::catalogCount(p, slot, petui::CatalogFilter::Unowned) == 0);
    for (unsigned at = 0; at < expected; ++at)
      assert(pet::objectAvailable(petui::catalogItem(p, slot, petui::CatalogFilter::Owned, at)));
  }
  std::array<uint8_t, pet::STATE_BYTES> bytes{};
  for (unsigned id : RETIRED) {
    assert(!pet::chooseProject(p, id) && !pet::equip(p, id));
    pet::Profile old;
    old.created = true;
    old.readingSeconds = 72000;
    old.projectStartedSeconds = 67890;
    old.project = uint8_t(id);
    storedOwn(old, 3);
    storedOwn(old, id);
    old.equipped[1] = uint8_t(id);
    pet::encode(old, bytes.data());
    pet::Profile migrated;
    assert(pet::decode(bytes.data(), migrated));
    assert(migrated.equipped[1] == 255 && migrated.project == 4 && !pet::owns(migrated, id));
    assert(migrated.projectStartedSeconds == 67890 && migrated.readingSeconds == 72000);
    pet::encode(migrated, bytes.data());
    pet::Profile again;
    assert(pet::decode(bytes.data(), again) && again.project == 4 && again.equipped[1] == 255);
    const unsigned ordinal = id < 12 ? id % 3 : 3 + (id - 12) % 57;
    assert(pet::momentObject(ordinal << 6, 1) == 255);
  }
  for (unsigned id = 0; id < pet::OBJECT_COUNT; ++id) {
    if (pet::objectSlot(id) != 3) continue;
    const auto area = petui::habitatlayout::item(id);
    const auto& source = petui::OBJECT_ART[id];
    assert(area.width >= source.width && area.height >= source.height);
    assert(area.width <= (source.anchor == 5 ? 84 : 112) && area.height <= 72 && area.x >= 0 && area.right() <= 432);
    assert(area.y >= 0 && area.bottom() <= 216);
    if (source.anchor == 5) assert(area.bottom() == 205);
  }
  const auto leaves = petui::habitatlayout::item(9);
  assert(leaves.width >= 45 && leaves.height >= 35);
  std::puts(
      "PET catalogue: 17 retired IDs, 223 available, legacy saves and reading credit, memories, enlarged scenery PASS");
}
