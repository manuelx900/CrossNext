#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../../src/clippings/ClippingText.h"
#include "Vocabulary.h"

int checks = 0;
#define CHECK(x)                                      \
  do {                                                \
    ++checks;                                         \
    if (!(x)) {                                       \
      std::fprintf(stderr, "%d: %s\n", __LINE__, #x); \
      std::exit(1);                                   \
    }                                                 \
  } while (0)
using namespace vocabulary;
struct Fake final : Records {
  std::map<std::string, Word> words;
  std::map<std::string, Context> contexts;
  int failure = 0, writes = 0;
  bool readFailure = false;
  Read word(const char* id, Word& w) override {
    if (readFailure) return Read::Error;
    auto i = words.find(id);
    if (i == words.end()) return Read::Missing;
    w = i->second;
    return Read::Ok;
  }
  Read context(const char* id, Context& c) override {
    if (readFailure) return Read::Error;
    auto i = contexts.find(id);
    if (i == contexts.end()) return Read::Missing;
    c = i->second;
    return Read::Ok;
  }
  bool put(const Word& w) override {
    if (++writes == failure) return false;
    words[w.id] = w;
    return true;
  }
  bool put(const Context& c) override {
    if (++writes == failure) return false;
    contexts[c.id] = c;
    return true;
  }
  bool removeWord(const char* id) override {
    if (++writes == failure) return false;
    words.erase(id);
    return true;
  }
  bool removeContext(const char* id) override {
    if (++writes == failure) return false;
    contexts.erase(id);
    return true;
  }
};
void fixture(Word& w, Context& c) {
  w = {};
  c = {};
  copy(w.surface, sizeof(w.surface), "glanced");
  copy(w.headword, sizeof(w.headword), "glance");
  copy(w.meaning, sizeof(w.meaning), "Echar un vistazo");
  copy(w.dictionary, sizeof(w.dictionary), "English-Spanish");
  copy(w.language, sizeof(w.language), "en");
  copy(c.path, sizeof(c.path), "/books/Harbor.epub");
  copy(c.book, sizeof(c.book), "The quiet harbor");
  copy(c.chapter, sizeof(c.chapter), "Chapter 1");
  copy(c.sentence, sizeof(c.sentence), "She glanced at the door.");
  c.offset = 1234;
  c.spine = 2;
  c.page = 8;
}
int main() {
  Fake db;
  Word incoming, old;
  Context ctx, oldContext;
  Undo token;
  fixture(incoming, ctx);
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Saved);
  CHECK(db.words.size() == 1 && db.contexts.size() == 1 && token.valid);
  const std::string id = incoming.id, first = ctx.id;
  CHECK(db.words[id].card.isNew());
  CHECK(db.words[id].encounters == 1);
  study::Fsrs fsrs;
  study::Scheduler scheduler(fsrs, study::Steps::defaults());
  db.words[id].card = scheduler.answer(db.words[id].card, study::Rating::Easy, 20000, 100).card;
  const auto card = db.words[id].card;
  CHECK(card.reps == 1 && !card.isNew());
  fixture(incoming, ctx);
  copy(incoming.surface, sizeof(incoming.surface), "glances");
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Updated);
  CHECK(db.words.size() == 1 && db.contexts.size() == 1 && incoming.encounters == 2);
  CHECK(incoming.card.stability == card.stability && incoming.card.dueDay == card.dueDay &&
        incoming.card.reps == card.reps);
  CHECK(undo(db, token, old));
  CHECK(db.words[id].encounters == 1);
  CHECK(db.words[id].card.reps == 1);
  CHECK(!undo(db, token, old));
  fixture(incoming, ctx);
  ctx.spine = 4;
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Updated);
  CHECK(db.contexts.size() == 2 && incoming.card.reps == 1);
  CHECK(undo(db, token, old));
  CHECK(db.contexts.size() == 1 && !std::strcmp(db.words[id].latest, first.c_str()));
  db.words[id].known = true;
  fixture(incoming, ctx);
  copy(ctx.path, sizeof(ctx.path), "/books/Other.epub");
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Updated);
  CHECK(incoming.known && incoming.card.reps == 1 && db.contexts.size() == 2);
  CHECK(undo(db, token, old));
  CHECK(db.words[id].known);
  fixture(incoming, ctx);
  copy(incoming.dictionary, sizeof(incoming.dictionary), "English-French");
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Saved);
  CHECK(db.words.size() == 2);
  CHECK(undo(db, token, old));
  CHECK(db.words.size() == 1);
  for (int fail = 1; fail <= 2; ++fail) {
    Fake broken;
    broken.failure = fail;
    fixture(incoming, ctx);
    CHECK(capture(broken, incoming, ctx, old, oldContext, token) == Capture::Failed);
    CHECK(!token.valid && broken.words.empty() && broken.contexts.empty());
  }
  Fake unreadable;
  unreadable.readFailure = true;
  fixture(incoming, ctx);
  CHECK(capture(unreadable, incoming, ctx, old, oldContext, token) == Capture::Failed);
  CHECK(unreadable.writes == 0);
  fixture(incoming, ctx);
  incoming.meaning[0] = 0;
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Rejected);
  fixture(incoming, ctx);
  ctx.sentence[0] = 0;
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Rejected);
  fixture(incoming, ctx);
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Updated);
  std::vector<uint8_t> bytes(kWireBytes);
  incoming.lastRating = 2;
  incoming.failures = 7;
  const size_t n = encode(incoming, bytes.data(), bytes.size());
  CHECK(n > 2000 && n <= kWireBytes);
  CHECK(decode(bytes.data(), n, old));
  CHECK(!std::strcmp(old.meaning, incoming.meaning) && old.card.stability == incoming.card.stability);
  CHECK(old.lastRating == 2 && old.failures == 7);
  {
    auto legacy = bytes;
    legacy.erase(legacy.begin() + n - 12, legacy.begin() + n - 4);
    legacy[4] = 1;
    const auto crc = checksum(legacy.data(), n - 12);
    for (int i = 0; i < 4; ++i) legacy[n - 12 + i] = (crc >> (i * 8)) & 255;
    CHECK(decode(legacy.data(), n - 8, old));
    CHECK(old.lastRating == 0 && old.failures == 0);
    CHECK(old.card.reps == incoming.card.reps && old.card.stability == incoming.card.stability);
  }
  CHECK(!decode(bytes.data(), n - 1, old));
  CHECK(!decode(bytes.data(), n, oldContext));
  for (size_t i = 0; i < n; ++i) {
    bytes[i] ^= 0x40;
    CHECK(!decode(bytes.data(), n, old));
    bytes[i] ^= 0x40;
  }
  const size_t m = encode(ctx, bytes.data(), bytes.size());
  CHECK(decode(bytes.data(), m, oldContext));
  CHECK(oldContext.offset == ctx.offset && !std::strcmp(oldContext.sentence, ctx.sentence));
  for (size_t i = 0; i < m; ++i) {
    bytes[i] ^= 1;
    CHECK(!decode(bytes.data(), m, oldContext));
    bytes[i] ^= 1;
  }
  const Word constant = incoming;
  CHECK(encode(constant, bytes.data(), bytes.size()) == n);
  char tiny[4];
  CHECK(!copy(tiny, sizeof(tiny), "éé"));
  CHECK(!std::strcmp(tiny, "é"));
  char text[128];
  cloze("She glanced at the door.", "glanced", text, sizeof(text));
  CHECK(!std::strcmp(text, "She [...] at the door."));
  cloze("GLANCED", "glanced", text, sizeof(text));
  CHECK(!std::strcmp(text, "[...]"));
  cloze("short", "longer", text, sizeof(text));
  CHECK(!std::strcmp(text, "short"));
  cloze("She walked, he glanced.", "he", text, sizeof(text));
  CHECK(!std::strcmp(text, "She walked, [...] glanced."));
  cloze("He said he would.", "he", text, sizeof(text));
  CHECK(!std::strcmp(text, "[...] said [...] would."));
  copy(ctx.surface, sizeof(ctx.surface), "glanced");
  copy(ctx.sentence, sizeof(ctx.sentence), "She glanced at the door.");
  ctx.fragment = true;
  reviewQuestion(ctx, true, "[Fragment] ", text, sizeof(text));
  CHECK(!std::strcmp(text, "[Fragment] She [...] at the door."));
  reviewQuestion(ctx, true, "[Fragmento] ", text, sizeof(text));
  CHECK(!std::strcmp(text, "[Fragmento] She [...] at the door."));
  reviewQuestion(ctx, false, "[Fragmento] ", text, sizeof(text));
  CHECK(!std::strcmp(text, "glanced"));
  ctx.fragment = false;
  reviewQuestion(ctx, true, "[Fragmento] ", text, sizeof(text));
  CHECK(!std::strcmp(text, "She [...] at the door."));
  CHECK(clippingText::exportHeapReady(6144, 6144, 4096, 1, 0));
  CHECK(!clippingText::exportHeapReady(6144, 6144, 4096, 1, 240));
  CHECK(!clippingText::exportHeapReady(12000, 8192, 4096, 1, 240));
  CHECK(clippingText::exportHeapReady(12000, 12000, 4096, 1, 240));
  fixture(incoming, ctx);
  copy(incoming.meaning, sizeof(incoming.meaning), " \n\t");
  CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Rejected);
  CHECK(sentenceEnd("door.\""));
  CHECK(sentenceEnd("fin！"));
  CHECK(!sentenceEnd("hello"));
  for (int i = 0; i < 1000; ++i) {
    fixture(incoming, ctx);
    std::snprintf(incoming.headword, sizeof(incoming.headword), "word-%d", i);
    CHECK(capture(db, incoming, ctx, old, oldContext, token) == Capture::Saved);
  }
  CHECK(db.words.size() == 1001);
  // A translation creates a separate source note without resetting the word's FSRS state.
  Fake purposeDb;
  fixture(incoming, ctx);
  CHECK(capture(purposeDb, incoming, ctx, old, oldContext, token) == Capture::Saved);
  const auto definitionContext = ctx;
  incoming.card.reps = 17;
  incoming.card.stability = 22;
  incoming.known = true;
  CHECK(purposeDb.put(incoming));
  ctx.purpose = Purpose::Language;
  CHECK(capture(purposeDb, incoming, ctx, old, oldContext, token) == Capture::Updated);
  CHECK(std::strcmp(ctx.id, definitionContext.id));
  CHECK(incoming.card.reps == 17 && incoming.card.stability == 22 && incoming.known);
  CHECK(purposeDb.words.size() == 1 && purposeDb.contexts.size() == 2);
  CHECK(encode(ctx, bytes.data(), bytes.size()) == m);
  CHECK(decode(bytes.data(), m, oldContext) && oldContext.purpose == Purpose::Language);
  CHECK(undo(purposeDb, token, old));
  CHECK(purposeDb.words.size() == 1 && purposeDb.contexts.size() == 1);
  CHECK(old.card.reps == 17 && old.card.stability == 22);
  // Legacy v1 source records have no purpose field and keep their original IDs.
  const size_t legacySize = encode(definitionContext, bytes.data(), bytes.size()) - 4;
  bytes[4] = 1;
  const auto crc = checksum(bytes.data(), legacySize - 4);
  for (int i = 0; i < 4; ++i) bytes[legacySize - 4 + i] = crc >> (i * 8);
  CHECK(decode(bytes.data(), legacySize, oldContext));
  CHECK(oldContext.purpose == Purpose::Vocabulary && !std::strcmp(oldContext.id, definitionContext.id));
  std::printf("%d vocabulary checks, 0 failures; Word=%zu Context=%zu wire=%zu/%zu\n", checks, sizeof(Word),
              sizeof(Context), n, m);
}
