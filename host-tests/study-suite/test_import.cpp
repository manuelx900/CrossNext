#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>

#include "../../src/apps_local/flashcards/FlashImport.h"
#include "../../src/apps_local/intercept/InterceptCore.h"
#include "../../src/apps_local/notes/NotesLibrary.h"
#include "../../src/apps_local/quest/QuestCore.h"
#include "../../src/apps_local/study/StudyDeck.h"
#include "../../src/apps_local/tools/ToolState.h"
#include "stubs/HalStorage.h"
static unsigned checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                               \
    }                                             \
  } while (0)
class Bytes : public study::ByteSource {
 public:
  const std::vector<uint8_t>& data;
  explicit Bytes(const char* path) : data(testStorage.files.at(path)) {}
  bool read(uint32_t at, void* out, uint32_t n) override {
    if (at > data.size() || n > data.size() - at) return false;
    std::memcpy(out, data.data() + at, n);
    return true;
  }
  uint32_t size() const override { return data.size(); }
};
static void load(const char* path, const std::string& file) {
  std::ifstream input(file, std::ios::binary);
  CHECK(input);
  testStorage.files[path].assign(std::istreambuf_iterator<char>(input), {});
}
struct ProgressRecord {
  unsigned phases[7]{};
  bool bounded = true;
  static void receive(void* context, const flashcards::ImportProgress& p) {
    auto& record = *static_cast<ProgressRecord*>(context);
    ++record.phases[static_cast<unsigned>(p.phase)];
    record.bounded &= !p.total || p.done <= p.total;
  }
};
static void runImport(const char* source, const char* target, unsigned expected) {
  auto importer = std::make_unique<flashcards::Importer>();
  ProgressRecord progress;
  importer->setProgressReporter({&progress, ProgressRecord::receive});
  CHECK(importer->begin(source, target, "Test deck"));
  flashcards::ImportResult result = flashcards::ImportResult::Busy;
  for (int n = 0; n < 1000 && result == flashcards::ImportResult::Busy; ++n) result = importer->step();
  CHECK(result == flashcards::ImportResult::Complete);
  CHECK(importer->count() == expected);
  CHECK(progress.bounded);
  CHECK(progress.phases[static_cast<unsigned>(flashcards::ImportPhase::Cards)] > 0);
  CHECK(progress.phases[static_cast<unsigned>(flashcards::ImportPhase::Saving)] > 0);
  importer.reset();
  const auto prefix = std::string(target) + "/";
  Bytes meta((prefix + "meta.dat").c_str()), deck((prefix + "deck.dat").c_str()), cards((prefix + "cards.dat").c_str());
  study::StudyDeck parsed;
  CHECK(parsed.openMeta(meta));
  CHECK(parsed.openDeck(deck));
  CHECK(parsed.noteCount() == int(expected));
  study::Note note;
  std::vector<uint8_t> noteBuffer(study::kMaxNoteBytes);
  note.useBuffer(noteBuffer.data(), noteBuffer.size());
  study::CardState card;
  for (unsigned n = 0; n < expected; ++n) {
    CHECK(parsed.loadNote(deck, n, note));
    CHECK(!note.isCloze());
    CHECK(!note.empty(study::Field::Meaning));
    CHECK(parsed.loadCard(cards, n, card));
    CHECK(card.state == 0);
  }
}
int main(int argc, char** argv) {
  CHECK(argc == 2);
  CHECK(Storage.ensureDirectoryExists("/.crosspoint"));
  CHECK(Storage.ensureDirectoryExists("/.crosspoint/flashcards"));
  {
    const char* folder = "/.crosspoint/flashcards/media";
    CHECK(Storage.ensureDirectoryExists(folder));
    const std::string map = "{\"0\":\"picture test.png\",\"1\":\"artery&vein.png\"}";
    testStorage.files["/images.apkg::media"] = {map.begin(), map.end()};
    testStorage.files["/images.apkg::0"] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    testStorage.files["/images.apkg::1"] = testStorage.files["/images.apkg::0"];
    flashcards::FlashMedia media;
    CHECK(media.begin("/images.apkg", folder));
    media.prepare("<img src='picture%20test.png'>", nullptr);
    CHECK(media.question() && !media.answer());
    CHECK(media.append());
    media.reverse();
    CHECK(!media.question() && media.answer());
    CHECK(media.append());
    media.prepare("<img src='artery&amp;vein.png'>", nullptr);
    CHECK(media.question());
    CHECK(media.append());
    CHECK(media.finish(3));
    Bytes imagesFile("/.crosspoint/flashcards/media/images.dat");
    study::StudyImages images;
    CHECK(images.open(imagesFile));
    CHECK(images.at(imagesFile, 0, false).valid() && !images.at(imagesFile, 0).valid());
    CHECK(!images.at(imagesFile, 1, false).valid() && images.at(imagesFile, 1).valid());
    uint8_t rows[study::kImageBandBytes];
    CHECK(images.readBand(imagesFile, images.at(imagesFile, 0, false), 0, rows) == 2);
    CHECK(rows[0] == 0xbf && rows[1] == 0x7f);
    media.prepare("<img src='missing.png'>", nullptr);
    CHECK(!media.question());
  }
  {
    load("/regressions.anki2", std::string(argv[1]) + "/anki-regressions.sqlite");
    runImport("/regressions.anki2", "/.crosspoint/flashcards/regressions", 2);
    Bytes file("/.crosspoint/flashcards/regressions/deck.dat");
    study::StudyDeck deck;
    study::Note note;
    CHECK(deck.openDeck(file));
    CHECK(deck.loadNote(file, 0, note));
    CHECK(std::string(note.field(study::Field::Headword)) == "Energy: [...]");
    CHECK(std::string(note.field(study::Field::Meaning)) == "Energy: mc^2\nExtra explanation");
    CHECK(deck.loadNote(file, 1, note));
    CHECK(std::string(note.field(study::Field::Headword)) == "after");
  }
  {
    load("/long.anki2", std::string(argv[1]) + "/anki-long.sqlite");
    runImport("/long.anki2", "/.crosspoint/flashcards/long", 4);
    Bytes file("/.crosspoint/flashcards/long/deck.dat");
    study::StudyDeck deck;
    study::Note note;
    std::vector<uint8_t> memory(study::kMaxNoteBytes);
    CHECK(deck.openDeck(file) && deck.hasLongNotes());
    CHECK(!deck.loadNote(file, 1, note));  // an inline-only caller refuses, never truncates
    note.useBuffer(memory.data(), memory.size());
    CHECK(deck.loadNote(file, 1, note));
    CHECK(note.length(study::Field::Headword) == 20000);
    CHECK(note.length(study::Field::Meaning) == 50000);
    CHECK(std::string(note.field(study::Field::Headword)) == std::string(20000, 'Q'));
    CHECK(std::string(note.field(study::Field::Meaning)) == std::string(50000, 'A'));
    CHECK(deck.loadNote(file, 3, note));
    CHECK(std::string(note.field(study::Field::Headword)) == "after");
  }
  std::string csv = "#separator:comma\nFront,Back\n\"Quoted, question\",\"Answer \"\"quoted\"\"\"\nOne,Two\n";
  testStorage.files["/demo.csv"] = {csv.begin(), csv.end()};
  runImport("/demo.csv", "/.crosspoint/flashcards/csv", 2);
  const std::string tsv = "Hello, world; punctuation\tThe answer\n";
  testStorage.files["/punctuation.tsv"] = {tsv.begin(), tsv.end()};
  runImport("/punctuation.tsv", "/.crosspoint/flashcards/tsv", 1);
  {
    Bytes deck("/.crosspoint/flashcards/tsv/deck.dat");
    study::StudyDeck parsed;
    study::Note note;
    CHECK(parsed.openDeck(deck));
    CHECK(parsed.loadNote(deck, 0, note));
    CHECK(std::string(note.field(study::Field::Headword)) == "Hello, world; punctuation");
  }
  // Oversized quoted records remain cancellable, including an EOF without a closing quote.
  std::string huge = "\"" + std::string(2 * 1024 * 1024, 'a');
  testStorage.files["/huge.csv"] = {huge.begin(), huge.end()};
  {
    auto importer = std::make_unique<flashcards::Importer>();
    CHECK(importer->begin("/huge.csv", "/.crosspoint/flashcards/cancelled", "Cancelled"));
    testStorage.bytesRead = 0;
    CHECK(importer->step() == flashcards::ImportResult::Busy);
    CHECK(testStorage.bytesRead <= 4096);
  }
  for (const auto& file : testStorage.files) CHECK(file.first.find("/.crosspoint/flashcards/cancelled/") != 0);
  std::string recovered = huge + "\",ignored\nValid,Answer\n";
  testStorage.files["/oversize.csv"] = {recovered.begin(), recovered.end()};
  runImport("/oversize.csv", "/.crosspoint/flashcards/recovered", 1);
  // A record boundary and doubled quotes can straddle separate conversion batches.
  const std::string boundary = "\"" + std::string(4093, 'b') + "\"\"\",answer\nNext,card\n";
  testStorage.files["/boundary.csv"] = {boundary.begin(), boundary.end()};
  runImport("/boundary.csv", "/.crosspoint/flashcards/boundary", 2);
  load("/raw.anki2", std::string(argv[1]) + "/anki-512.sqlite");
  runImport("/raw.anki2", "/.crosspoint/flashcards/sql", 300);  // SQLite overflow note is retained
  load("/old.apkg::collection.anki2", std::string(argv[1]) + "/anki-1024.sqlite");
  runImport("/old.apkg", "/.crosspoint/flashcards/old", 300);
  load("/modern.apkg::collection.anki21b", std::string(argv[1]) + "/anki-zstd.bin");
  runImport("/modern.apkg", "/.crosspoint/flashcards/modern", 300);
  {
    auto importer = std::make_unique<flashcards::Importer>();
    CHECK(importer->begin("/modern.apkg", "/.crosspoint/flashcards/abandoned", "Abandoned"));
    CHECK(Storage.exists("/.crosspoint/flashcards/abandoned/.collection"));
    CHECK(Storage.exists("/.crosspoint/flashcards/abandoned/.sqlite"));
  }
  for (const auto& file : testStorage.files) CHECK(file.first.find("/.crosspoint/flashcards/abandoned/") != 0);
  const auto completed = testStorage.files["/.crosspoint/flashcards/modern/cards.dat"];
  {
    flashcards::Importer importer;
    CHECK(!importer.begin("/modern.apkg", "/.crosspoint/flashcards/modern", "Existing"));
  }
  CHECK(testStorage.files["/.crosspoint/flashcards/modern/cards.dat"] == completed);
  {
    auto importer = std::make_unique<flashcards::Importer>();
    CHECK(importer->begin("/demo.csv", "/.crosspoint/flashcards/writefail", "Write failure"));
    testStorage.writeLimit = 1;
    CHECK(importer->step() == flashcards::ImportResult::Failed);
    testStorage.writeLimit = -1;
  }
  for (const auto& file : testStorage.files) CHECK(file.first.find("/.crosspoint/flashcards/writefail/") != 0);
  {
    notes::Library all;
    std::string message;
    CHECK(all.begin());
    CHECK(all.create("Shopping", message));
    CHECK(all.save("Shopping", "- [ ] Milk\n", message));
    notes::Library prose;
    prose.filterTasks(false);
    CHECK(prose.begin() && prose.count() == 0);
    const auto saved = testStorage.files["/notes/Shopping.md"];
    CHECK(!prose.create("Shopping", message));
    CHECK(testStorage.files["/notes/Shopping.md"] == saved);
    CHECK(prose.create("Diary", message));
    CHECK(!prose.rename("Diary", "Shopping", message));
    CHECK(testStorage.files["/notes/Shopping.md"] == saved);
    notes::Library tasks;
    tasks.filterTasks(true);
    CHECK(tasks.begin() && tasks.count() == 1);
    CHECK(!tasks.create("Diary", message));
    CHECK(!tasks.rename("Shopping", "Diary", message));
    CHECK(all.create("Empty", message));
    CHECK(all.save("Empty", "- [ ] \n", message));
    tasks.scan();
    CHECK(tasks.count() == 2);
    CHECK(tasks.entries()[0].name == "Empty" && tasks.entries()[0].total == 0);
    prose.scan();
    CHECK(prose.count() == 1 && prose.entries()[0].name == "Diary");
  }
  tools::State tool;
  bool writable;
  CHECK(tools::load(tool, writable) && writable);
  tool.count = 123;
  CHECK(tools::save(tool, writable));
  tools::State restored;
  CHECK(tools::load(restored, writable) && restored.count == 123);
  tool.count = 124;
  testStorage.writeLimit = 12;
  CHECK(!tools::save(tool, writable));
  testStorage.writeLimit = -1;
  CHECK(tools::load(restored, writable) && restored.count == 123);
  quest::StoreWork questWork;
  quest::Profile profile;
  profile.xp = 1000;
  CHECK(quest::save(profile, true, questWork));
  quest::Profile p2;
  CHECK(quest::load(p2, writable, questWork) && p2.xp == 1000);
  profile.xp = 2000;
  testStorage.writeLimit = 20;
  CHECK(!quest::save(profile, true, questWork));
  testStorage.writeLimit = -1;
  CHECK(quest::load(p2, writable, questWork) && p2.xp == 1000);
  // Notification acknowledgement is durable and never awards currency twice.
  CHECK(quest::hasLevelUp(p2));
  const auto beforeAcknowledgement = quest::coins(p2);
  profile = p2;
  CHECK(quest::acknowledgeLevel(profile));
  testStorage.writeLimit = 20;
  CHECK(!quest::save(profile, writable, questWork));
  testStorage.writeLimit = -1;
  CHECK(quest::load(p2, writable, questWork) && quest::hasLevelUp(p2));
  CHECK(quest::coins(p2) == beforeAcknowledgement);
  CHECK(quest::acknowledgeLevel(p2) && quest::save(p2, writable, questWork));
  CHECK(quest::load(profile, writable, questWork) && !quest::hasLevelUp(profile));
  CHECK(quest::coins(profile) == beforeAcknowledgement);
  {
    testStorage.files.erase("/.crosspoint/reading-quest-v5.dat");
    auto& legacy = testStorage.files["/.crosspoint/reading-quest.dat"];
    legacy.assign(128, 0);
    const auto put = [](uint8_t* p, uint64_t value, int bytes) {
      for (int n = 0; n < bytes; ++n) p[n] = uint8_t(value >> (n * 8));
    };
    for (int slot = 0; slot < 2; ++slot) {
      auto* b = legacy.data() + slot * 64;
      std::memcpy(b, "RQU1", 4);
      put(b + 4, 100 + slot, 4);
      put(b + 8, 8880, 8);
      put(b + 16, 220, 8);
      put(b + 24, 120, 4);
      put(b + 28, 20000, 4);
      b[32] = 7;
      b[33] = 2;
      put(b + 60, readingstats::checksum(b, 60), 4);
    }
    const auto original = legacy;
    CHECK(quest::load(p2, writable, questWork) && writable && p2.sequence == 101 && p2.xp == 8880);
    CHECK(p2.spent == 220 && p2.bonus == 120 && p2.claimedDay == 20000);
    CHECK(p2.equipped[0] == 4 && p2.equipped[1] == quest::kGearCount && p2.equipped[2] == quest::kGearCount);
    testStorage.writeLimit = 20;
    CHECK(!quest::save(p2, writable, questWork));
    testStorage.writeLimit = -1;
    CHECK(!Storage.exists("/.crosspoint/reading-quest-v5.dat"));
    CHECK(quest::load(p2, writable, questWork) && p2.sequence == 101 && p2.xp == 8880);
    CHECK(quest::save(p2, writable, questWork) && p2.sequence == 1);
    CHECK(testStorage.files["/.crosspoint/reading-quest.dat"] == original);
    p2.gender = quest::Gender::Feminine;
    CHECK(quest::save(p2, writable, questWork) && p2.sequence == 2);
    CHECK(quest::load(profile, writable, questWork) && profile.gender == quest::Gender::Feminine && profile.xp == 8880);
    testStorage.files["/.crosspoint/reading-quest-v5.dat"][3] = '9';
    const auto future = testStorage.files["/.crosspoint/reading-quest-v5.dat"];
    CHECK(!quest::load(profile, writable, questWork) && !writable);
    CHECK(!quest::save(profile, writable, questWork));
    CHECK(testStorage.files["/.crosspoint/reading-quest-v5.dat"] == future);
  }
  {
    testStorage.files.erase("/.crosspoint/reading-quest-v5.dat");
    quest::Profile old;
    old.xp = 10000;
    old.spent = 300;
    old.bonus = 80;
    old.gender = quest::Gender::Feminine;
    old.owned[1] = 4;
    old.equipped[0] = 10;
    uint8_t bytes[quest::kProfileBytes]{};
    quest::encode(old, bytes);
    std::memcpy(bytes, "RQU4", 4);
    bytes[252] = 0;
    const auto crc = readingstats::checksum(bytes, 316);
    for (int n = 0; n < 4; ++n) bytes[316 + n] = crc >> (8 * n);
    auto& v4 = testStorage.files["/.crosspoint/reading-quest-v4.dat"];
    v4.assign(bytes, bytes + sizeof(bytes));
    const auto original = v4;
    CHECK(quest::load(p2, writable, questWork) && writable);
    CHECK(p2.xp == 10000 && p2.spent == 300 && p2.bonus == 80);
    CHECK(p2.gender == quest::Gender::Feminine && quest::owns(p2, 10) && p2.equipped[0] == 10);
    CHECK(!quest::hasLevelUp(p2));
    CHECK(quest::coins(p2) > old.xp / 10 - old.spent);
    CHECK(quest::save(p2, writable, questWork));
    CHECK(v4 == original);
    CHECK(quest::load(profile, writable, questWork) && !quest::hasLevelUp(profile));
    testStorage.files.erase("/.crosspoint/reading-quest-v4.dat");
  }
  {
    // Revalued historical reading can grant new levels once on migration.
    testStorage.files.erase("/.crosspoint/reading-quest-v5.dat");
    quest::Profile old;
    old.xp = 75;
    uint8_t bytes[quest::kProfileBytes]{};
    quest::encode(old, bytes);
    std::memcpy(bytes, "RQU4", 4);
    bytes[252] = 0;
    const auto crc = readingstats::checksum(bytes, 316);
    for (int n = 0; n < 4; ++n) bytes[316 + n] = crc >> (8 * n);
    auto& v4 = testStorage.files["/.crosspoint/reading-quest-v4.dat"];
    v4.assign(bytes, bytes + sizeof(bytes));
    CHECK(quest::load(p2, writable, questWork) && !quest::hasLevelUp(p2));
    readingstats::Summary history;
    history.seconds = 1800;
    history.totalDays = 1;
    quest::refresh(p2, history);
    CHECK(p2.xp == 105 && quest::pendingLevelCoins(p2) == 24);
    CHECK(quest::save(p2, writable, questWork));
    CHECK(quest::load(profile, writable, questWork) && quest::hasLevelUp(profile));
    const auto balance = quest::coins(profile);
    CHECK(quest::acknowledgeLevel(profile) && quest::save(profile, writable, questWork));
    CHECK(quest::load(p2, writable, questWork) && !quest::hasLevelUp(p2));
    quest::refresh(p2, history);
    CHECK(!quest::hasLevelUp(p2) && quest::coins(p2) == balance);
    testStorage.files.erase("/.crosspoint/reading-quest-v4.dat");
  }
  for (int version : {2, 3}) {
    testStorage.files.erase("/.crosspoint/reading-quest-v5.dat");
    testStorage.files.erase("/.crosspoint/reading-quest-v3.dat");
    uint8_t encoded[quest::kProfileBytes]{};
    const auto putOld = [&](int at, uint64_t value, int bytes) {
      for (int n = 0; n < bytes; ++n) encoded[at + n] = uint8_t(value >> (n * 8));
    };
    std::memcpy(encoded, version == 3 ? "RQU3" : "RQU2", 4);
    putOld(4, 43, 4);
    putOld(8, 1000000000, 8);
    putOld(16, 123456, 8);
    putOld(24, 60, 4);
    putOld(28, 20001, 4);
    encoded[32] = 1;
    const int worn[] = {49, 99, 149, 349, 374};
    for (int n = 0; n < 5; ++n) putOld(33 + n * 2, worn[n], 2);
    std::memset(encoded + 43, 255, 47);
    encoded[89] = 127;
    encoded[90] = 1;
    encoded[214] = 128;
    if (version == 3) encoded[277] = 8;
    const int checksumAt = version == 3 ? 316 : 252;
    putOld(checksumAt, readingstats::checksum(encoded, checksumAt), 4);
    const char* oldPath = version == 3 ? "/.crosspoint/reading-quest-v3.dat" : "/.crosspoint/reading-quest-v2.dat";
    auto& oldFile = testStorage.files[oldPath];
    oldFile.assign(encoded, encoded + (version == 3 ? quest::kProfileBytes : quest::kV2ProfileBytes));
    const auto original = oldFile;
    CHECK(quest::load(p2, writable, questWork) && writable && p2.sequence == 43);
    CHECK(p2.xp == 1000000000 && p2.spent == 123456 && p2.bonus == 60 && p2.gender == quest::Gender::Feminine);
    CHECK(p2.claimedDay == 20001 && quest::isClaimed(p2, 0) && quest::isClaimed(p2, 999));
    CHECK(quest::isClaimed(p2, 1499) == (version == 3));
    for (int n = 1000; n < 1499; ++n) CHECK(!quest::isClaimed(p2, n));
    const int migratedPets[] = {3,  5,  4,  1,  0,  6,  7,  8,  2,  9,  10, 11, 12,
                                13, 14, 32, 30, 39, 33, 40, 42, 41, 46, 47, 49};
    for (int n = 0; n < quest::kGearCount; ++n) {
      bool expected = n < 100 ? n % 2 == 0 : n < 150;
      if (n >= 150)
        for (int pet : migratedPets) expected |= n == 150 + pet;
      CHECK(quest::owns(p2, n) == expected);
    }
    CHECK(p2.equipped[0] == 98 && p2.equipped[1] == 149 && p2.equipped[2] == 199);
    testStorage.writeLimit = 20;
    CHECK(!quest::save(p2, writable, questWork));
    testStorage.writeLimit = -1;
    CHECK(oldFile == original);
    CHECK(quest::load(p2, writable, questWork) && p2.sequence == 43);
    CHECK(quest::save(p2, writable, questWork) && p2.sequence == 1);
    CHECK(quest::load(profile, writable, questWork) && quest::isClaimed(profile, 999));
    CHECK(oldFile == original);
    testStorage.files.erase("/.crosspoint/reading-quest-v5.dat");
    oldFile[checksumAt - 1] ^= 1;
    CHECK(!quest::load(profile, writable, questWork) && !writable);
  }
  intercept::State story;
  CHECK(intercept::choose(story, 0));
  CHECK(intercept::save(story, true));
  intercept::State s2;
  CHECK(intercept::load(s2, writable) && s2.node == story.node);
  testStorage.files["/.crosspoint/intercept.dat"][3] = '2';
  CHECK(!intercept::load(s2, writable) && !writable);
  const auto before = testStorage.files["/.crosspoint/intercept.dat"];
  CHECK(!intercept::save(s2, writable));
  CHECK(before == testStorage.files["/.crosspoint/intercept.dat"]);
  std::printf("PASS: %u CSV/Anki/zstd conversion and durable store checks\n", checks);
}
