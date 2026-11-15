#include <HalStorage.h>

#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/pet/PetHistory.h"
#include "../../src/apps_local/pet/PetStore.h"

namespace {
constexpr const char* A = "/.crosspoint/pet-v2/state-a.bin";
constexpr const char* B = "/.crosspoint/pet-v2/state-b.bin";
void generationIsolation() {
  pet::Store store;
  pet::HistoryStore lives;
  pet::Profile old;
  old.created = true;
  old.generation = 9;
  old.readingSeconds = 90000;
  std::strcpy(old.name, "OLD");
  std::vector<uint8_t> bytes(pet::STATE_BYTES);
  pet::encode(old, bytes.data());
  testStorage.files["/.crosspoint/pet/state-a.bin"] = bytes;
  testStorage.files["/.crosspoint/pet/state-b.bin"] = bytes;
  pet::History archive;
  assert(pet::endLife(old, pet::LifeStatus::Retired, 1700000000));
  assert(pet::addLife(archive, old));
  bytes.resize(pet::HISTORY_BYTES);
  pet::encodeHistory(archive, bytes.data());
  testStorage.files["/.crosspoint/pet/lives-a.bin"] = bytes;
  testStorage.files["/.crosspoint/pet/lives-b.bin"] = bytes;
  testStorage.files["/.crosspoint/reading-history/sessions.bin"] = {1, 2, 3};
  const auto originals = testStorage.files;
  pet::Profile fresh, loaded;
  pet::History history, loadedHistory;
  bool writable = false;
  assert(store.load(fresh, writable) && writable && !fresh.created && !fresh.readingSeconds);
  assert(lives.load(history, writable) && writable && history.count == 0);
  assert(testStorage.files == originals && testStorage.writes == 0);
  fresh.created = true;
  fresh.generation = 1;
  fresh.readingSeconds = 60;
  std::strcpy(fresh.name, "NEW");
  assert(store.save(fresh, writable));
  assert(store.load(loaded, writable) && loaded.generation == 1 && loaded.readingSeconds == 60);
  assert(!std::strcmp(loaded.name, "NEW"));
  assert(pet::endLife(fresh, pet::LifeStatus::Retired, 1700100000));
  assert(pet::addLife(history, fresh) && lives.save(history, writable));
  assert(lives.load(loadedHistory, writable) && loadedHistory.count == 1);
  assert(loadedHistory.records[0].generation == 1 && !std::strcmp(loadedHistory.records[0].name, "NEW"));
  testStorage.files[B][0] = 0;
  assert(!store.load(loaded, writable) && !writable);
  testStorage.files["/.crosspoint/pet-v2/lives-b.bin"][0] = 0;
  assert(!lives.load(loadedHistory, writable) && !writable);
  for (const auto& file : originals) assert(testStorage.files.at(file.first) == file.second);
  testStorage = {};
}
}  // namespace
int main() {
  generationIsolation();
  pet::Store store;
  pet::Profile p;
  bool writable = false;
  assert(store.load(p, writable) && writable && !p.created);
  std::strcpy(p.name, "LUNA");
  p.created = true;
  p.bornDay = 20000;
  p.readingSeconds = 1200;
  assert(store.save(p, writable) && p.sequence == 1);
  assert(testStorage.files.count(B) && !testStorage.files.count(A));
  p.saciety = 80;
  assert(store.save(p, writable) && p.sequence == 2);
  const auto good = testStorage.files[A];
  pet::Profile restored;
  assert(store.load(restored, writable) && restored.sequence == 2 && restored.saciety == 80);
  assert(!std::strcmp(restored.name, "LUNA"));
  testStorage.files[A][100] ^= 1;
  assert(store.load(restored, writable) && restored.sequence == 1 && restored.saciety == 70);
  testStorage.files[A] = good;
  testStorage.writeLimit = 8;
  p.saciety = 90;
  assert(!store.save(p, writable) && p.sequence == 2);
  assert(store.load(restored, writable) && restored.sequence == 2 && restored.saciety == 80);
  testStorage.writeLimit = -1;
  testStorage.corruptReadChecksum = true;
  assert(!store.save(p, writable) && p.sequence == 2);
  testStorage.corruptReadChecksum = false;
  assert(store.load(restored, writable) && restored.sequence == 3 && restored.saciety == 90);
  testStorage.available = false;
  assert(!store.save(p, writable));
  testStorage.available = true;
  testStorage.files[B].assign(400, 0);
  testStorage.files[B][0] = 'P';
  testStorage.files[B][1] = 'E';
  testStorage.files[B][2] = 'T';
  testStorage.files[B][3] = ':';
  assert(!store.load(restored, writable) && !writable);
  const int writes = testStorage.writes;
  assert(!store.save(p, writable) && testStorage.writes == writes);
  testStorage.files.clear();
  pet::HistoryStore lifeStore;
  pet::History history, loadedHistory;
  assert(lifeStore.load(history, writable) && writable && !history.count);
  p.lifeStatus = pet::LifeStatus::Retired;
  p.endedAt = 1700000000;
  {
    static_assert(sizeof(pet::LifeRecord) == 56);
    pet::Profile aged = p;
    aged.generation = 1;
    aged.bornDay = 20000;
    aged.sharedDays = 3;
    pet::History ages, roundtrip;
    uint8_t bytes[pet::HISTORY_BYTES];
    for (const int days : {0, 1, 23, 70000}) {
      const int endedDay = aged.bornDay + days;
      aged.endedAt = int64_t(endedDay) * 86400;
      assert(pet::addLife(ages, aged, endedDay));
      assert(!pet::addLife(ages, aged, endedDay));
      pet::encodeHistory(ages, bytes);
      assert(pet::decodeHistory(bytes, roundtrip));
      assert(roundtrip.count == 1 && roundtrip.records[0].ageKnown);
      assert(roundtrip.records[0].ageDays == std::min(days, int(UINT16_MAX)) && roundtrip.records[0].days == 3);
    }
    const auto seal = [&]() {
      const auto crc = pet::checksum(bytes, sizeof(bytes) - 4);
      for (unsigned i = 0; i < 4; ++i) bytes[sizeof(bytes) - 4 + i] = uint8_t(crc >> (8 * i));
    };
    bytes[12 + 49] = bytes[12 + 50] = bytes[12 + 51] = 0;
    seal();
    assert(pet::decodeHistory(bytes, roundtrip) && !roundtrip.records[0].ageKnown);
    bytes[12 + 51] = 2;
    seal();
    assert(!pet::decodeHistory(bytes, roundtrip));
    assert(!pet::addLife(ages, aged) && ages.records[0].ageKnown);
    assert(!pet::addLife(ages, aged, aged.bornDay + 2) && ages.records[0].ageDays == UINT16_MAX);
  }
  for (unsigned i = 1; i <= 14; ++i) {
    p.generation = i;
    assert(pet::addLife(history, p));
  }
  assert(history.count == 12 && history.records[0].generation == 3);
  assert(!pet::addLife(history, p));
  p.readingSeconds += 1200;
  assert(pet::addLife(history, p) && history.count == 12 && history.records[11].readingSeconds == p.readingSeconds);
  assert(lifeStore.save(history, writable));
  assert(lifeStore.load(loadedHistory, writable) && loadedHistory.count == 12);
  assert(!std::strcmp(loadedHistory.records[11].name, "LUNA"));
  p.generation = 15;
  assert(pet::addLife(history, p));
  testStorage.writeLimit = 8;
  assert(!lifeStore.save(history, writable));
  testStorage.writeLimit = -1;
  assert(lifeStore.load(loadedHistory, writable) && loadedHistory.records[11].generation == 14);
  assert(lifeStore.save(history, writable));
  assert(lifeStore.load(loadedHistory, writable) && loadedHistory.records[11].generation == 15);
  testStorage.files["/.crosspoint/pet-v2/lives-a.bin"][40] ^= 1;
  assert(lifeStore.load(loadedHistory, writable) && loadedHistory.records[11].generation == 14);
  testStorage.files["/.crosspoint/pet-v2/lives-a.bin"][3] = '3';
  assert(!lifeStore.load(loadedHistory, writable) && !writable);
  assert(!lifeStore.save(history, writable));
  testStorage.files.clear();
  pet::History oldHistory;
  for (unsigned i = 1; i <= 12; ++i) {
    p.generation = i;
    assert(pet::addLife(oldHistory, p));
  }
  std::vector<uint8_t> modern(pet::HISTORY_BYTES), legacy(pet::LEGACY_HISTORY_BYTES, 0);
  pet::encodeHistory(oldHistory, modern.data());
  std::copy(modern.begin(), modern.begin() + 12, legacy.begin());
  legacy[3] = '1';
  for (unsigned i = 0; i < 12; ++i)
    std::copy(modern.begin() + 12 + i * 52, modern.begin() + 36 + i * 52, legacy.begin() + 12 + i * 32);
  const auto checksum = pet::checksum(legacy.data(), legacy.size() - 4);
  for (unsigned i = 0; i < 4; ++i) legacy[legacy.size() - 4 + i] = uint8_t(checksum >> (8 * i));
  testStorage.files["/.crosspoint/pet-v2/lives-a.bin"] = legacy;
  assert(lifeStore.load(loadedHistory, writable) && writable && loadedHistory.count == 12);
  for (unsigned i = 0; i < 12; ++i)
    assert(!loadedHistory.records[i].name[0] && loadedHistory.records[i].generation == i + 1 &&
           !loadedHistory.records[i].ageKnown);
  assert(lifeStore.save(loadedHistory, writable));
  assert(lifeStore.load(loadedHistory, writable) && loadedHistory.count == 12);
  testStorage.files.clear();
  pet::Profile lost;
  assert(store.load(lost, writable) && writable);
  lost.created = true;
  lost.generation = 3;
  assert(pet::endLife(lost, pet::LifeStatus::Lost, 1700600000));
  assert(store.save(lost, writable));
  assert(store.load(restored, writable) && pet::deathPending(restored) && restored.generation == 3);
  auto acknowledged = restored;
  assert(pet::acknowledgeDeath(acknowledged));
  testStorage.writeLimit = 8;
  assert(!store.save(acknowledged, writable));
  assert(store.load(restored, writable) && pet::deathPending(restored));
  testStorage.writeLimit = -1;
  assert(store.save(acknowledged, writable));
  assert(store.load(restored, writable) && !pet::deathPending(restored) && restored.deathAcknowledged &&
         restored.generation == 3);
  auto futureMemory = testStorage.files[A];
  if (futureMemory.empty()) futureMemory = testStorage.files[B];
  testStorage.files[A] = futureMemory;
  testStorage.files[A][pet::MOMENTS_VERSION_OFFSET] ^= 2;
  assert(store.load(restored, writable) && writable && restored.sequence == 1 && pet::deathPending(restored));
  futureMemory[pet::MOMENTS_VERSION_OFFSET] = 3;
  const auto memoryChecksum = pet::checksum(futureMemory.data(), pet::STATE_BYTES - 4);
  for (unsigned i = 0; i < 4; ++i) futureMemory[pet::STATE_BYTES - 4 + i] = uint8_t(memoryChecksum >> (8 * i));
  testStorage.files[A] = futureMemory;
  assert(!store.load(restored, writable) && !writable);
  const int memoryWrites = testStorage.writes;
  assert(!store.save(restored, writable) && testStorage.writes == memoryWrites);
  // A PET5 backup remains usable if the first larger PET6 write is interrupted.
  testStorage.files.clear();
  p = {};
  p.created = true;
  p.sequence = 9;
  p.readingSeconds = 1200;
  std::vector<uint8_t> expanded(pet::STATE_BYTES), old(pet::LEGACY_STATE_BYTES);
  pet::encode(p, expanded.data());
  std::copy(expanded.begin(), expanded.begin() + pet::LEGACY_STATE_BYTES, old.begin());
  old[3] = '5';
  old[205] = 1;
  std::fill(old.begin() + 206, old.begin() + 252, 0);
  const auto oldChecksum = pet::checksum(old.data(), old.size() - 4);
  for (unsigned i = 0; i < 4; ++i) old[old.size() - 4 + i] = uint8_t(oldChecksum >> (8 * i));
  testStorage.files[B] = old;
  assert(store.load(restored, writable) && restored.sequence == 9 && !restored.gamesWon);
  restored.gamesWon = 24;
  testStorage.writeLimit = 8;
  assert(!store.save(restored, writable));
  assert(store.load(p, writable) && p.sequence == 9 && !p.gamesWon && testStorage.files[B] == old);
  testStorage.writeLimit = -1;
  assert(store.save(restored, writable) && testStorage.files[A].size() == pet::STATE_BYTES);
  assert(store.load(p, writable) && p.sequence == 10 && p.gamesWon == 24);
  std::puts("PET storage: all checks passed");
}
