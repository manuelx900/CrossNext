#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/pet/PetState.h"
#include "../../src/apps_local/quest/QuestCore.h"
int main() {
  const char* name = "Лукʼян";
  assert(pet::validName(name, false));
  assert(quest::validName(name, false));
  pet::Profile petProfile;
  petProfile.created = true;
  std::snprintf(petProfile.name, sizeof(petProfile.name), "%s", name);
  uint8_t petBytes[pet::STATE_BYTES];
  pet::encode(petProfile, petBytes);
  pet::Profile petReloaded;
  assert(pet::decode(petBytes, petReloaded));
  assert(!std::strcmp(petReloaded.name, name));
  quest::Profile questProfile;
  std::snprintf(questProfile.name, sizeof(questProfile.name), "%s", name);
  uint8_t questBytes[quest::kProfileBytes];
  quest::encode(questProfile, questBytes);
  quest::Profile questReloaded;
  assert(quest::decode(questBytes, questReloaded));
  assert(!std::strcmp(questReloaded.name, name));
  std::puts("Ukrainian name with U+02BC preserved by actual PET and Quest encode/decode");
}
