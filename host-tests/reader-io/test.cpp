#include <Epub/WordWidthCache.h>
#include <Epub/blocks/TextBlock.h>
#include <ReadBuffer.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <builtinFonts/ubuntu_12_regular.h>
#pragma GCC diagnostic pop
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#define CHECK(c)                                  \
  do {                                            \
    if (!(c)) {                                   \
      std::printf("FAIL %d: %s\n", __LINE__, #c); \
      std::exit(1);                               \
    }                                             \
  } while (0)
struct Result {
  size_t reads, bytes, logical, zero;
};
Result decode(const std::vector<uint8_t>& bytes, bool buffered, bool annotated) {
  HalFile input;
  input.bytes = bytes;
  std::array<uint8_t, 4096> storage;
  serialization::ReadBuffer reader(input, buffered ? storage.data() : nullptr, storage.size());
  HalFile roundtrip;
  for (int line = 0; line < 30; ++line) {
    auto block = TextBlock::deserialize(reader);
    CHECK(block && block->wordCount() == 10);
    CHECK(block->serialize(roundtrip));
    CHECK(block->hasRuby() == (annotated && line % 3 == 0));
  }
  CHECK(reader.finish());
  CHECK(roundtrip.bytes == bytes && input.position() == bytes.size());
  return {input.backendReads, input.backendBytes, input.logicalReads, input.zeroLengthReads};
}
void page(bool annotated) {
  HalFile output;
  const std::vector<std::string> words = {"Una",    "página",  "con",     "texto", "conserva",
                                          "España", "avances", "estilos", "y",     "kerning"};
  std::vector<int16_t> positions;
  positions.reserve(10);
  std::vector<EpdFontFamily::Style> styles;
  styles.reserve(10);
  for (int n = 0; n < 10; ++n) {
    positions.push_back(n * 34);
    styles.push_back(n % 3 == 0 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
  }
  for (int line = 0; line < 30; ++line) {
    BlockStyle style;
    style.marginLeft = line % 2;
    style.characterSpacing = line % 3;
    style.isRtl = false;
    std::vector<std::string> ruby;
    std::vector<uint8_t> focus;
    std::vector<uint16_t> split;
    if (annotated) {
      focus.resize(10);
      split.resize(10);
      focus[2] = 3;
      split[2] = 24;
      if (line % 3 == 0) {
        ruby.resize(10);
        ruby[1] = "ふりがな";
      }
    }
    TextBlock block(words, positions, styles, focus, split, style, ruby);
    CHECK(block.valid());
    CHECK(block.serialize(output));
  }
  serialization::ReadBuffer memory(output.bytes.data(), output.bytes.size());
  HalFile roundtrip;
  for (int i = 0; i < 30; ++i) {
    auto block = TextBlock::deserialize(memory);
    CHECK(block && block->serialize(roundtrip));
  }
  CHECK(roundtrip.bytes == output.bytes && memory.finish() && memory.available() == 0);
  uint8_t byte;
  CHECK(memory.read(&byte, 1) == 0);
  serialization::ReadBuffer empty(static_cast<const uint8_t*>(nullptr), 123);
  CHECK(empty.available() == 0 && empty.read(&byte, 1) == 0 && empty.finish());
  serialization::ReadBuffer truncated(output.bytes.data(), 1);
  CHECK(!TextBlock::deserialize(truncated));
  auto a = decode(output.bytes, false, annotated), b = decode(output.bytes, true, annotated);
  CHECK(a.bytes == b.bytes && b.reads < a.reads);
  std::printf(
      "{\"case\":\"%s\",\"lines\":30,\"words\":300,\"payload_bytes\":%zu,\"baseline_reads\":%zu,\"prototype_backend_"
      "reads\":%zu,\"logical_reads\":%zu,\"baseline_zero_length_calls\":%zu,\"roundtrip_identical\":true}\n",
      annotated ? "styled-focus-ruby" : "styled-latin", output.bytes.size(), a.reads, b.reads, b.logical, a.zero);
}
void metrics() {
  WordWidthCache cache;
  WordWidthCache::Key key{17, 0, 4, 0, 0, 100, 0};
  uint16_t value = 0;
  CHECK(!cache.get("España", key, value));
  cache.put("España", key, 123);
  CHECK(cache.get("España", key, value) && value == 123);
  for (int n = 0; n < 7; ++n) {
    auto other = key;
    if (n == 0) ++other.font;
    if (n == 1) ++other.fontId;
    if (n == 2) ++other.style;
    if (n == 3) ++other.focus;
    if (n == 4) ++other.spacing;
    if (n == 5) ++other.tracking;
    if (n == 6) ++other.emphasis;
    CHECK(!cache.get("España", other, value));
  }
  CHECK(!cache.get("Espana", key, value));
  CHECK(!cache.get(std::string(100, 'a'), key, value));
  HalMemory::fail = true;
  WordWidthCache oom;
  CHECK(!oom.get("España", key, value));
  oom.put("España", key, 9);
  CHECK(!oom.get("España", key, value));
  HalMemory::fail = false;
  std::puts("Exact word metrics: identity, style, focus, spacing, collisions/length and OOM checks passed");
}
void bufferBounds() {
  HalFile input;
  input.bytes.resize(1000);
  for (size_t i = 0; i < input.bytes.size(); ++i) input.bytes[i] = static_cast<uint8_t>(i);
  std::array<uint8_t, 4096> scratch{};
  std::array<uint8_t, 64> result{};
  serialization::ReadBuffer page(input, scratch.data(), scratch.size(), 37);
  CHECK(page.available() == 37);
  CHECK(page.read(result.data(), result.size()) == 37);
  CHECK(std::memcmp(result.data(), input.bytes.data(), 37) == 0);
  CHECK(page.available() == 0 && page.read(result.data(), 1) == 0);
  CHECK(input.backendReads == 1 && input.backendBytes == 37);
  CHECK(page.finish() && input.position() == 37);
  CHECK(input.read() == 37);
  std::puts("Read-ahead stays inside the page extent and restores the exact file cursor");
}
int main() {
  bufferBounds();
  page(false);
  page(true);
  metrics();
}
