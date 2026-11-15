#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../../src/apps_local/pet/PetState.h"
#include "../../src/apps_local/quest/QuestCore.h"
int main() {
  for (const char* name : {u8"Маша", u8"มะลิ", u8"小花", u8"さくら", u8"Léa", u8"Nguyễn", "Luna", "A1", "LUNA-A"}) {
    pet::Profile pet, restoredPet;
    std::strcpy(pet.name, name);
    assert(pet::validName(pet.name, false));
    uint8_t petBytes[pet::STATE_BYTES];
    pet::encode(pet, petBytes);
    assert(petBytes[3] == '9');
    assert(pet::decode(petBytes, restoredPet) && !std::strcmp(restoredPet.name, name));
    quest::Profile quest, restoredQuest;
    std::strcpy(quest.name, name);
    assert(quest::validName(quest.name, false));
    uint8_t questBytes[quest::kProfileBytes];
    quest::encode(quest, questBytes);
    assert(questBytes[3] == '7');
    assert(quest::decode(questBytes, restoredQuest) && !std::strcmp(restoredQuest.name, name));
  }
  std::puts("Unicode PET9 and RQU7: six alphabets round-trip without truncation");
}
