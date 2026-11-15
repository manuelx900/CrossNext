#pragma once
#include <cstdint>

enum class EpubEngine : uint8_t { Modern, Classic };

inline EpubEngine epubEngineFromSetting(uint8_t value) {
  return value == static_cast<uint8_t>(EpubEngine::Classic) ? EpubEngine::Classic : EpubEngine::Modern;
}

// The resolved text-rendering configuration a reader hands to the layout
// engine. Section-cache validation keys on every field: a section file built
// with a different spec is discarded and rebuilt.
//
// Build one via CrossPointSettings::readerRenderSpec(width, height), which
// fills every field: the settings-derived ones from the store, the viewport
// from the caller. Taking the viewport as arguments is what keeps a spec from
// existing in a half-filled state — the 0 defaults below are a last-resort
// backstop (a 0x0 viewport lays out nothing), not an invitation to omit it.
struct ReaderRenderSpec {
  EpubEngine engine = EpubEngine::Modern;
  int fontId = 0;
  uint32_t fontContentHash = 0;
  float lineCompression = 1.2f;  // Line height as a multiple of the font size in pixels.
  bool extraParagraphSpacing = false;
  uint8_t paragraphIndentSpaces = 2;
  int8_t characterSpacing = 0;
  uint8_t wordSpacingPercent = 100;
  uint8_t paragraphAlignment = 0;
  uint16_t viewportWidth = 0;
  uint16_t viewportHeight = 0;
  bool hyphenationEnabled = false;
  bool embeddedStyle = true;
  uint8_t imageRendering = 0;
  bool focusReadingEnabled = false;
  bool forceBold = false;
  bool operator==(const ReaderRenderSpec&) const = default;

  // Stable app IDs identify the slot; resource content identifies its layout.
  int fontCacheKey() const {
    const int key =
        fontContentHash == 0 ? fontId : static_cast<int>((static_cast<uint32_t>(fontId) ^ fontContentHash) * 16777619U);
    return engine == EpubEngine::Classic ? static_cast<int>(static_cast<uint32_t>(key) ^ 0x434C4153U) : key;
  }
};

// Stable signature for clipping anchors. Page-local word ordinals are valid
// only for the exact layout that produced them.
inline uint32_t readerRenderSpecSignature(const ReaderRenderSpec& spec) {
  uint32_t signature = 2166136261U;
  const auto mix = [&signature](const uint32_t value) {
    signature ^= value;
    signature *= 16777619U;
  };
  // Bump when the same spec values lay out differently, so old clipping word ranges stop matching.
  constexpr uint32_t LAYOUT_SIGNATURE_VERSION = 2;
  mix(LAYOUT_SIGNATURE_VERSION);
  mix(static_cast<uint32_t>(spec.fontId));
  mix(spec.fontContentHash);
  mix(static_cast<uint32_t>(spec.lineCompression * 1000.0f));
  mix(spec.extraParagraphSpacing);
  mix(static_cast<uint8_t>(spec.characterSpacing));
  mix(spec.wordSpacingPercent);
  mix(spec.paragraphAlignment);
  mix(spec.viewportWidth);
  mix(spec.viewportHeight);
  mix(spec.hyphenationEnabled);
  mix(spec.embeddedStyle);
  mix(spec.imageRendering);
  mix(spec.focusReadingEnabled);
  mix(spec.forceBold);
  // Preserve existing modern layouts and clipping signatures across this update.
  if (spec.engine == EpubEngine::Classic) mix(0x434C4153U);
  return signature == 0 ? 1 : signature;
}
