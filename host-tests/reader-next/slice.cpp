#include <Epub/ReaderRenderSpec.h>
#include <Performance.h>

#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
#define LOG_ERR(...) ((void)0)
#define CHECK(x)                                  \
  do {                                            \
    if (!(x)) {                                   \
      std::printf("slice failed %d\n", __LINE__); \
      return 1;                                   \
    }                                             \
  } while (0)
uint32_t nowMs = 0;
uint32_t millis() { return nowMs; }
struct ChapterParser {
  enum class ParseStatus { More, Done, Error };
};
struct Parser {
  int steps = 0, total = 30;
  uint32_t cost = 3;
  int* pages;
  std::vector<int> output;
  ChapterParser::ParseStatus parseStep() {
    ++steps;
    nowMs += cost;
    if (steps % 5 == 0) {
      ++*pages;
      output.push_back(*pages);
    }
    return steps == total ? ChapterParser::ParseStatus::Done : ChapterParser::ParseStatus::More;
  }
  bool finishParse() { return true; }
  bool hadEventCacheReadError() const { return false; }
  size_t parseBytesConsumed() const { return steps * 1024; }
};
struct Section {
  struct Build {
    std::unique_ptr<Parser> parser;
    struct Writer {
      bool needs = false;
      int chunks = 0;
      bool spoolNeedsDrain() const { return needs; }
      void allowPersistence() {
        needs = false;
        chunks = 3;
      }
      bool flushing() const { return chunks > 0; }
      bool flushSome() {
        --chunks;
        return true;
      }
      bool deferPersistence() { return true; }
    } writer;
    bool parserFinished = false, preview = false, formatBuild = false;
    bool cacheWriteFailed = false;
    ReaderRenderSpec spec;
    size_t bytesConsumed = 0;
  };
  int builtPageCount_ = 0;
  std::unique_ptr<Build> build_;
  bool done = false;
  Section() {
    build_ = std::make_unique<Build>();
    build_->parser = std::make_unique<Parser>();
    build_->parser->pages = &builtPageCount_;
  }
  bool buildSomeMore(int, bool (*)() = nullptr, uint32_t = 0);
  void suspendBuild() { build_.reset(); }
  bool startBuildImpl(const ReaderRenderSpec&, void*, bool) { return false; }
  void abandonBuild() { suspendBuild(); }
  void discardBuild() { suspendBuild(); }
  bool finalizeBuild() {
    done = true;
    return true;
  }
};
#include "slice-under-test.inc"
int main() {
  Section direct, sliced;
  CHECK(direct.buildSomeMore(0));
  const auto expected = direct.build_->parser->output;
  nowMs = 0;
  int calls = 0;
  while (!sliced.done) {
    const auto start = nowMs;
    CHECK(sliced.buildSomeMore(2, nullptr, 8));
    CHECK(uint32_t(nowMs - start) <= 9);
    ++calls;
  }
  CHECK(calls > 2);
  CHECK(sliced.build_->parser->output == expected);
  CHECK(sliced.builtPageCount_ == direct.builtPageCount_);
  Section wrapped;
  nowMs = UINT32_MAX - 2;
  CHECK(wrapped.buildSomeMore(2, nullptr, 8));
  CHECK(wrapped.build_->parser->steps == 3);
  Section cooperative;
  cooperative.build_->parser->cost = 40;
  nowMs = 0;
  CHECK(cooperative.buildSomeMore(2, nullptr, 8));
  CHECK(cooperative.build_->parser->steps == 1);
  CHECK(nowMs == 40);
  Section callback;
  CHECK(callback.buildSomeMore(0, []() { return true; }));
  CHECK(callback.build_->parser->steps == 1);
  Section spill;
  spill.build_->writer.needs = true;
  for (int chunk = 0; chunk < 3; ++chunk) {
    CHECK(spill.buildSomeMore(1, nullptr, 8));
    CHECK(spill.build_->parser->steps == 0);
  }
  CHECK(spill.buildSomeMore(1, nullptr, 8) && spill.build_->parser->steps > 0);
  Section preview;
  preview.build_->preview = true;
  CHECK(preview.buildSomeMore(0));
  CHECK(preview.build_->parserFinished && !preview.done);
  const int steps = preview.build_->parser->steps;
  CHECK(preview.buildSomeMore(1) && preview.build_->parser->steps == steps && !preview.done);
  preview.build_->preview = false;
  CHECK(preview.buildSomeMore(1) && preview.done);
  std::printf(
      "Production build slices: identical completed pages, wrap-safe budget, cooperative overrun and callback "
      "checks\n");
}
