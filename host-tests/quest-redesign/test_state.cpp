#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/quest/QuestCore.h"
#include "../study-suite/stubs/HalStorage.h"

int main() {
  quest::StoreWork work;
  quest::Profile profile, loaded;
  bool writable = false, found = true;
  testStorage.files.clear();
  assert(quest::load(profile, writable, work, &found) && writable && !found);
  assert(quest::owns(profile, quest::first(quest::Slot::Background)));
  assert(quest::price(quest::first(quest::Slot::Background)) == 0);
  profile.gender = quest::Gender::Feminine;
  for (char key : {'P', 'E', '[', 'A', ' '}) assert(quest::typeName(profile.name, key));
  assert(quest::finishName(profile.name) && std::strcmp(profile.name, "PEÑA") == 0);
  assert(quest::save(profile, writable, work));
  assert(quest::load(loaded, writable, work, &found) && found);
  assert(loaded.gender == quest::Gender::Feminine && loaded.sequence == 1);
  assert(std::strcmp(loaded.name, "PEÑA") == 0);
  assert(quest::eraseName(loaded.name) && quest::eraseName(loaded.name));
  assert(std::strcmp(loaded.name, "PE") == 0);
  char boundary[quest::NAME_BYTES]{};
  for (unsigned n = 0; n < quest::NAME_LIMIT; ++n) assert(quest::typeName(boundary, '['));
  assert(!quest::typeName(boundary, 'A') && quest::validName(boundary));
  assert(quest::nameLength(boundary) == quest::NAME_LIMIT);
  assert(!quest::validName(" BAD") && !quest::validName("BAD  NAME") && !quest::validName("BAD!"));
  assert(quest::validName("BAD1"));
  uint8_t oldV5[quest::kProfileBytes];
  quest::encode(profile, oldV5);
  std::memset(oldV5 + 253, 0, 26);
  auto recheck = [](uint8_t* b) {
    const int at = b[3] == '6' || b[3] == '7' ? 508 : 316;
    const auto value = readingstats::checksum(b, at);
    for (int n = 0; n < 4; ++n) b[at + n] = value >> (n * 8);
  };
  recheck(oldV5);
  assert(quest::decode(oldV5, loaded) && !loaded.name[0] && loaded.gender == profile.gender);
  oldV5[278] = 1;
  oldV5[253] = 0x80;
  recheck(oldV5);
  assert(!quest::decode(oldV5, loaded));
  std::memset(oldV5 + 253, 'A', quest::NAME_BYTES);
  recheck(oldV5);
  assert(!quest::decode(oldV5, loaded));

  profile.xp = 50000;
  profile.noticedLevel = quest::level(profile.xp);
  assert(quest::equip(profile, 149));
  assert(quest::equip(profile, 199));
  const auto coins = quest::coins(profile), spent = profile.spent;
  assert(profile.equipped[int(quest::Slot::Background)] == 149);
  uint8_t bytes[quest::kProfileBytes]{};
  quest::encode(profile, bytes);
  std::memcpy(bytes, "RQU4", 4);
  bytes[252] = 0;
  const auto crc = readingstats::checksum(bytes, 316);
  for (int n = 0; n < 4; ++n) bytes[316 + n] = crc >> (8 * n);
  testStorage.files.clear();
  auto& old = testStorage.files["/.crosspoint/reading-adventure-v2/reading-quest-v4.dat"];
  old.assign(bytes, bytes + sizeof(bytes));
  const auto original = old;
  assert(quest::load(loaded, writable, work, &found) && found && writable);
  assert(loaded.gender == profile.gender && loaded.xp == profile.xp);
  assert(!loaded.name[0]);
  assert(loaded.spent == spent && quest::coins(loaded) == coins);
  assert(loaded.equipped[int(quest::Slot::Background)] == 149 && quest::owns(loaded, 149));
  assert(loaded.equipped[int(quest::Slot::Pet)] == 199 && quest::owns(loaded, 199));
  assert(quest::save(loaded, writable, work) && old == original);
  assert(quest::load(profile, writable, work, &found) && found);
  assert(profile.gender == quest::Gender::Feminine && profile.spent == spent);
  const auto file = testStorage.files["/.crosspoint/reading-adventure-v2/reading-quest-v6.dat"];
  testStorage.files["/.crosspoint/reading-adventure-v2/reading-quest-v6.dat"][508] ^= 1;
  const auto damaged = testStorage.files["/.crosspoint/reading-adventure-v2/reading-quest-v6.dat"];
  assert(!quest::load(profile, writable, work, &found) && !writable && !found);
  assert(!quest::save(profile, writable, work));
  assert(testStorage.files["/.crosspoint/reading-adventure-v2/reading-quest-v6.dat"] == damaged);
  assert(file != damaged);
  for (int n = 0; n < quest::kBackgroundCount; ++n) {
    const int item = quest::first(quest::Slot::Background) + n;
    assert(quest::requiredLevel(item) >= 1 && quest::requiredLevel(item) <= 100);
    if (n) assert(quest::requiredLevel(item) > quest::requiredLevel(item - 1));
  }
  assert(quest::requiredLevel(149) == 100);
  std::puts(
      "Quest state: first-profile detection, starter background, sex persistence, legacy purchases and corrupt-save "
      "protection passed");
}
