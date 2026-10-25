#include <HalMemory.h>
#include <HalStorage.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "../../src/apps_local/flashcards/FlashImport.h"
#include "../../src/apps_local/study/StudyDeck.h"
#include "../../src/apps_local/study/StudyImages.h"
static unsigned checks = 0;
#define CHECK(x)                                  \
  do {                                            \
    ++checks;                                     \
    if (!(x)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #x); \
      std::exit(1);                               \
    }                                             \
  } while (0)
class Bytes : public study::WritableByteSource {
 public:
  std::vector<uint8_t>& data;
  explicit Bytes(const std::string& path) : data(testStorage.files.at(path)) {}
  bool read(uint32_t at, void* out, uint32_t n) override {
    if (at > data.size() || n > data.size() - at) return false;
    std::memcpy(out, data.data() + at, n);
    return true;
  }
  uint32_t size() const override { return data.size(); }
  bool write(uint32_t at, const void* value, uint32_t n) override {
    if (at > data.size() || n > data.size() - at) return false;
    std::memcpy(data.data() + at, value, n);
    return true;
  }
  bool flush() override { return true; }
};
static void load(const std::string& path, const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  CHECK(in.good());
  testStorage.files[path].assign(std::istreambuf_iterator<char>(in), {});
}
static flashcards::ImportResult complete(flashcards::Importer& importer) {
  auto result = flashcards::ImportResult::Busy;
  for (int n = 0; n < 200000 && result == flashcards::ImportResult::Busy; ++n) result = importer.step();
  return result;
}
int main(int argc, char** argv) {
  CHECK(argc == 3);
  CHECK(Storage.ensureDirectoryExists("/.crosspoint"));
  CHECK(Storage.ensureDirectoryExists("/.crosspoint/flashcards"));
  testStorage.files["/sentinel.txt"] = {'S', 'A', 'F', 'E'};
  std::printf("CASE\tRESULT\tIMPORTED\tOMITTED\tHOST_MS\tREAD_BYTES\n");
  unsigned serial = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[1])) {
    if (entry.path().extension() != ".apkg") continue;
    const auto name = entry.path().filename().string();
    const auto input = "/input/" + std::to_string(serial) + ".apkg";
    const auto target = "/.crosspoint/flashcards/test" + std::to_string(serial++);
    load(input, entry.path());
    auto before = testStorage.files[input];
    testStorage.bytesRead = 0;
    const auto start = std::chrono::steady_clock::now();
    auto importer = std::make_unique<flashcards::Importer>();
    bool began = importer->begin(input.c_str(), target.c_str(), name.c_str());
    auto result = began ? complete(*importer) : flashcards::ImportResult::Failed;
    const unsigned count = importer->count(), skipped = importer->skippedCount();
    importer.reset();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    std::printf("%s\t%s\t%u\t%u\t%lld\t%zu\n", name.c_str(),
                result == flashcards::ImportResult::Complete ? "COMPLETE" : "REJECTED", count, skipped, (long long)ms,
                testStorage.bytesRead);
    std::fflush(stdout);
    CHECK(testStorage.files[input] == before);
    CHECK(testStorage.files["/sentinel.txt"] == std::vector<uint8_t>({'S', 'A', 'F', 'E'}));
    CHECK(!Storage.exists("/escape.txt"));
    CHECK(!Storage.exists("/outside.png"));
    if (result != flashcards::ImportResult::Complete) {
      CHECK(!Storage.exists((target + "/meta.dat").c_str()));
      continue;
    }
    Bytes meta(target + "/meta.dat"), deck(target + "/deck.dat"), cards(target + "/cards.dat");
    study::StudyDeck parsed;
    CHECK(parsed.openMeta(meta) && parsed.openDeck(deck));
    CHECK(parsed.noteCount() == int(count));
    if (Storage.exists((target + "/images.dat").c_str())) {
      Bytes f(target + "/images.dat");
      study::StudyImages im;
      CHECK(im.open(f));
      auto ref = im.at(f, 0);
      if (name == "02_Imagenes_limites.apkg") CHECK(ref.width == 448 && ref.height == 336);
      if (name == "100_imagenes.apkg") CHECK(ref.width == 448 && ref.height == 448);
      if (name == "03_Cloze_y_plantillas.apkg") {
        CHECK(!im.at(f, 5, false).valid());
        CHECK(im.at(f, 5, true).valid());
      }
    }
    study::Note note;
    std::vector<uint8_t> buffer(study::kMaxNoteBytes);
    note.useBuffer(buffer.data(), buffer.size());
    std::ofstream audit(std::filesystem::path(argv[2]) / (name + ".txt"));
    for (unsigned n = 0; n < count; ++n) {
      CHECK(parsed.loadNote(deck, n, note));
      if (n < 50) {
        audit << n << " Q[" << note.length(study::Field::Headword) << "] " << note.field(study::Field::Headword)
              << "\nA[" << note.length(study::Field::Meaning) << "] " << note.field(study::Field::Meaning) << "\n\n";
      }
    }
    if (name == "01_Textos_y_formatos.apkg") {
      CHECK(parsed.loadNote(deck, 5, note));
      CHECK(note.length(study::Field::Headword) == 20000);
      CHECK(note.length(study::Field::Meaning) == 50004);
      CHECK(std::string(note.field(study::Field::Headword)) == std::string(20000, 'Q'));
      CHECK(std::string(note.field(study::Field::Meaning)) == std::string(50000, 'A') + " END");
    }
    if (name == "02_Imagenes_limites.apkg") CHECK(count == 7 && skipped == 10);
    if (name == "03_Cloze_y_plantillas.apkg") CHECK(count == 10 && skipped == 2);
    if (name == "30000_tarjetas.apkg") {
      CHECK(count == 30000);
      CHECK(parsed.loadNote(deck, count - 1, note));
      CHECK(std::string(note.field(study::Field::Headword)) == "Question 29999");
      for (unsigned n = 0; n < 500; ++n) {
        study::CardState state;
        CHECK(parsed.loadCard(cards, n, state));
        state.reps = 7;
        state.dueDay = 21;
        CHECK(parsed.storeCard(cards, n, state));
      }
      for (unsigned n = 0; n < 500; ++n) {
        study::CardState state;
        CHECK(parsed.loadCard(cards, n, state));
        CHECK(state.reps == 7 && state.dueDay == 21);
      }
      const auto saved = cards.data;
      auto again = std::make_unique<flashcards::Importer>();
      CHECK(!again->begin(input.c_str(), target.c_str(), "Reimport"));
      CHECK(cards.data == saved);
    }
  }
  // A failed or interrupted importer never publishes metadata. A fresh instance can retry.
  load("/retry.apkg", std::filesystem::path(argv[1]) / "Escala/00100_tarjetas.apkg");
  const char* retry = "/.crosspoint/flashcards/retry";
  testPsramFailure = true;
  {
    auto importer = std::make_unique<flashcards::Importer>();
    CHECK(!importer->begin("/retry.apkg", retry, "OOM"));
  }
  testPsramFailure = false;
  CHECK(!Storage.exists("/.crosspoint/flashcards/retry/meta.dat"));
  {
    auto importer = std::make_unique<flashcards::Importer>();
    CHECK(importer->begin("/retry.apkg", retry, "Interrupted"));
    CHECK(importer->step() == flashcards::ImportResult::Busy);
  }
  CHECK(!Storage.exists("/.crosspoint/flashcards/retry/meta.dat"));
  {
    auto importer = std::make_unique<flashcards::Importer>();
    CHECK(importer->begin("/retry.apkg", retry, "Short write"));
    testStorage.writeLimit = 1;
    CHECK(complete(*importer) == flashcards::ImportResult::Failed);
    testStorage.writeLimit = -1;
  }
  CHECK(!Storage.exists("/.crosspoint/flashcards/retry/meta.dat"));
  {
    auto importer = std::make_unique<flashcards::Importer>();
    CHECK(importer->begin("/retry.apkg", retry, "Retry"));
    CHECK(complete(*importer) == flashcards::ImportResult::Complete);
    CHECK(importer->count() == 100);
  }
  std::printf("CHECKS\t%u\nMAX_EXPLICIT_PSRAM_REQUEST\t%zu\n", checks, testLargestPsram);
}
