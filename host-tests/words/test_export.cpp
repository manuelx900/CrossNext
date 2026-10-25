#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "VocabularyExport.h"
using namespace vocabulary;
int checks = 0;
#define CHECK(x)                                      \
  do {                                                \
    ++checks;                                         \
    if (!(x)) {                                       \
      std::fprintf(stderr, "%d: %s\n", __LINE__, #x); \
      std::exit(1);                                   \
    }                                                 \
  } while (0)
struct Fixture {
  StorageRecords records;
  Word word;
  Context context;
  ExportJob job{records, word, context};
  Fixture() {
    testStorage = {};
    CHECK(StorageRecords::ready());
    CHECK(Storage.ensureDirectoryExists("/WORDS"));
  }
  void add(int n, int source, Purpose purpose) {
    word = {};
    context = {};
    char label[48];
    std::snprintf(label, sizeof(label), "word-%d", n);
    copy(word.headword, sizeof(word.headword), label);
    copy(word.surface, sizeof(word.surface), label);
    copy(word.meaning, sizeof(word.meaning), "A meaning\twith\nlines");
    copy(word.dictionary, sizeof(word.dictionary), "EN-ES");
    identify(word);
    copy(context.word, sizeof(context.word), word.id);
    copy(context.surface, sizeof(context.surface), label);
    copy(context.sentence, sizeof(context.sentence), "A saved sentence.");
    copy(context.book, sizeof(context.book), "Book");
    std::snprintf(label, sizeof(label), "/books/%d.epub", source);
    copy(context.path, sizeof(context.path), label);
    context.spine = source;
    context.purpose = purpose;
    identify(context);
    copy(word.latest, sizeof(word.latest), context.id);
    word.encounters = 1;
    CHECK(records.put(context));
    CHECK(records.put(word));
  }
  int run() {
    int steps = 0;
    while (!job.finished() && steps < 100000) {
      job.step(1);
      ++steps;
    }
    CHECK(job.finished());
    return steps;
  }
  std::string result() {
    auto& b = testStorage.files["/WORDS/vocabulary.tsv"];
    return {b.begin(), b.end()};
  }
};
int main() {
  {
    Fixture f;
    for (int n = 0; n < 1000; ++n) f.add(n, 0, Purpose::Vocabulary);
    testStorage.bytesRead = 0;
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1000));
    f.run();
    CHECK(f.job.succeeded());
    CHECK(testStorage.bytesRead < 4 * 1024 * 1024);
    CHECK(f.result().find("word-") == std::string::npos);
    CHECK(!Storage.exists("/WORDS/vocabulary.ids.tmp"));
  }
  {
    Fixture f;
    for (int n = 0; n < 80; ++n) {
      f.add(n, 0, Purpose::Vocabulary);
      f.add(n, 1, Purpose::Language);
      f.add(n, 2, Purpose::Language);
    }
    const auto words = testStorage.files;
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 160));
    f.run();
    CHECK(f.job.succeeded());
    const auto out = f.result();
    int lines = 0;
    for (char c : out)
      if (c == '\n') ++lines;
    CHECK(lines == 83);
    CHECK(out.find("A meaning with lines") != std::string::npos);
    for (auto& item : words)
      if (item.first.find(kWords) == 0) CHECK(testStorage.files[item.first] == item.second);
    CHECK(!Storage.exists("/WORDS/vocabulary.ids.tmp"));
    CHECK(f.job.begin(Purpose::Vocabulary, "/books/0.epub", 0, "EN-ES", 80));
    f.run();
    CHECK(f.job.succeeded());
    CHECK(f.result().find("word-79") != std::string::npos);
  }
  {
    Fixture f;
    for (int n = 0; n < 1000; ++n) f.add(n, 0, Purpose::Language);
    testStorage.bytesRead = 0;
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1000));
    f.run();
    CHECK(f.job.succeeded());
    CHECK(testStorage.bytesRead < 8 * 1024 * 1024);
    int lines = 0;
    for (char c : f.result())
      if (c == '\n') ++lines;
    CHECK(lines == 1003);
  }
  {
    Fixture f;
    f.add(1, 0, Purpose::Language);
    testStorage.files["/WORDS/vocabulary.tsv"] = {'o', 'l', 'd'};
    testStorage.files[std::string(kContexts) + "/" + f.context.id + ".dat"].back() ^= 1;
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1));
    f.run();
    CHECK(!f.job.succeeded());
    CHECK(f.result() == "old");
  }
  for (int stop : {0, 1, 10, 30}) {
    Fixture f;
    f.add(1, 0, Purpose::Language);
    testStorage.files["/WORDS/vocabulary.tsv"] = {'o', 'l', 'd'};
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1));
    for (int i = 0; i < stop && !f.job.finished(); ++i) f.job.step(1);
    if (f.job.finished()) continue;
    f.job.cancel();
    CHECK(f.job.idle());
    CHECK(f.result() == "old");
    CHECK(!Storage.exists("/WORDS/vocabulary.tsv.tmp"));
    CHECK(!Storage.exists("/WORDS/vocabulary.ids.tmp"));
  }
  for (int failure : {1, 2}) {
    Fixture f;
    f.add(1, 0, Purpose::Language);
    testStorage.files["/WORDS/vocabulary.tsv"] = {'o', 'l', 'd'};
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1));
    testStorage.renameCalls = 0;
    testStorage.failRenameAt = failure;
    f.run();
    CHECK(!f.job.succeeded());
    CHECK(f.result() == "old");
  }
  {
    Fixture f;
    f.add(1, 0, Purpose::Language);
    testStorage.files["/WORDS/vocabulary.tsv"] = {'o', 'l', 'd'};
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1));
    testStorage.writeLimit = 0;
    f.run();
    CHECK(!f.job.succeeded());
    CHECK(f.result() == "old");
  }
  {
    Fixture f;
    f.add(1, 0, Purpose::Language);
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1));
    testStorage.available = false;
    f.job.step();
    CHECK(f.job.finished() && !f.job.succeeded());
    testStorage.available = true;
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 1));
    f.run();
    CHECK(f.job.succeeded());
  }
  {
    Fixture f;
    testStorage.bytesRead = 0;
    CHECK(f.job.begin(Purpose::Language, "", -1, "", 0));
    CHECK(f.run() == 1);
    CHECK(f.job.succeeded());
    CHECK(testStorage.bytesRead == 0);
    CHECK(f.result().find("#columns:ID") != std::string::npos);
  }
  std::printf(
      "%d export checks, 0 failures (1000 opposite-collection records, dedup, cancellation, write/rename/SD "
      "failures)\n",
      checks);
}
