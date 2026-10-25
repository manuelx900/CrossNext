#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#include "../../src/apps_local/murdle/MurdleCampaign.h"
#include "../../src/apps_local/murdle/MurdleCast.h"
#include "../../src/apps_local/murdle/MurdleSave.h"
#include "../../src/apps_local/study/StudyDeck.h"
#include "../../src/apps_local/words/Vocabulary.h"
namespace fs = std::filesystem;
std::vector<uint8_t> read(const fs::path& path) {
  FILE* f = std::fopen(path.string().c_str(), "rb");
  if (!f) return {};
  std::fseek(f, 0, SEEK_END);
  std::vector<uint8_t> bytes(std::ftell(f));
  std::rewind(f);
  const auto count = std::fread(bytes.data(), 1, bytes.size(), f);
  std::fclose(f);
  if (count != bytes.size()) return {};
  return bytes;
}
struct Source : study::ByteSource {
  std::vector<uint8_t> bytes;
  explicit Source(const fs::path& p) : bytes(::read(p)) {}
  bool read(uint32_t at, void* out, uint32_t n) override {
    if (at > bytes.size() || n > bytes.size() - at) return false;
    std::memcpy(out, bytes.data() + at, n);
    return true;
  }
  uint32_t size() const override { return bytes.size(); }
};
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  const fs::path sd = argv[1];
  int words = 0, contexts = 0;
  for (const auto& p : fs::directory_iterator(sd / ".crosspoint/vocabulary/words")) {
    const auto bytes = read(p.path());
    vocabulary::Word w;
    if (!vocabulary::decode(bytes.data(), bytes.size(), w)) return 2;
    char id[33];
    std::memcpy(id, w.id, 33);
    vocabulary::identify(w);
    if (std::strcmp(id, w.id)) return 3;
    ++words;
  }
  for (const auto& p : fs::directory_iterator(sd / ".crosspoint/vocabulary/contexts")) {
    const auto bytes = read(p.path());
    vocabulary::Context c;
    if (!vocabulary::decode(bytes.data(), bytes.size(), c)) return 4;
    char id[33];
    std::memcpy(id, c.id, 33);
    vocabulary::identify(c);
    if (std::strcmp(id, c.id)) return 5;
    ++contexts;
  }
  Source meta(sd / "FLASHCARDS/English sample/meta.dat"), deck(sd / "FLASHCARDS/English sample/deck.dat"),
      cards(sd / "FLASHCARDS/English sample/cards.dat");
  study::StudyDeck parsed;
  if (!parsed.openMeta(meta) || !parsed.openDeck(deck) || parsed.noteCount() != 12) return 6;
  for (int n = 0; n < parsed.noteCount(); ++n) {
    study::Note note;
    study::CardState card;
    if (!parsed.loadNote(deck, n, note) || !parsed.loadCard(cards, n, card) || note.empty(study::Field::Headword) ||
        note.empty(study::Field::Meaning))
      return 7;
  }
  const auto bytes = read(sd / ".crosspoint/murdle.sav");
  murdlesave::State state{};
  if (!murdlesave::decode(bytes.data(), bytes.size(), state) || !state.hasCase) return 8;
  static murdle::Scratch scratch;
  static murdle::Puzzle puzzle;
  uint8_t cast[murdle::kMaxCats][murdle::kMaxItems]{};
  const auto tier = static_cast<murdle::Tier>(state.caseTier);
  if (!murdle::drawCast(state.seed, murdle::shapeOf(tier), cast) ||
      !murdle::generate(tier, state.seed, cast, murdle::attrMasksFor(cast, murdle::shapeOf(tier)), scratch, puzzle) ||
      murdle::fingerprint(puzzle) != state.fingerprint)
    return 9;
  std::printf("Fixtures: %d words, %d contexts, 12 readable flashcards and a reproducible Murdle case: PASS\n", words,
              contexts);
}
