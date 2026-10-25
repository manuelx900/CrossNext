# Horizontal monochrome glyph rasterization

`glyphBitmap::draw` clips the glyph to the logical view, physical panel and
buffered row band before decoding it. For 1-bit glyphs whose x axis advances
one physical bit left or right, the rasterizer combines kept pixels into a
destination-byte mask. Transparent pixels and bits outside the clip are preserved.
Each selected byte is updated once per glyph row. Descending rows reverse the
selected bits. Source rows remain bit-contiguous, including non-byte-aligned
widths; a second source byte is read only when selected pixels require it.

The specialized loop is kept outside the general rasterizer's inline body.
Other transforms and all 2-bit glyphs keep the existing pixel decoder. Normal
landscape UI text uses the specialization in both landscape orientations.
Ordinary portrait text does not; rotated labels can use it in portrait. Built-in
Noto Serif and current TrueType fonts are 2-bit, so this change does not accelerate
their glyph decoding. It does not change shaping, layout, storage, font caches,
display refresh or activity timing.

There is no new allocation, cache, lookup table, task or framebuffer. Only bounded
scalar locals and one call on the specialized path are added. Existing source and
destination buffer ownership stays with the caller. The code uses byte accesses.

## Verification

`test/glyph_bitmap/GlyphBitmapTest.cpp` compares the actual framebuffer with an
independent pixel reference across all four orientations, rotated text, both bit
depths, all planes, both ink states, logical clips and strip bands. The monochrome
matrix also varies widths 1..33, destination offsets 0..7 and empty, full,
single-edge and alternating bit patterns. Exact-sized source buffers and guarded
destination buffers are checked with address/undefined-behavior sanitizers in the
host QA run.

The QA benchmark compares the saved pre-change rasterizer with the candidate in
the same WebAssembly process under Node, using real uncompressed Ubuntu 10/12
glyphs, black/white writes and varied alignments. It alternates execution order
and reports the median of nine samples after warm-up. A synthetic 2-bit bitmap
and the portrait paths are controls. Timings cover the rasterizer only, not SD,
font decompression, page layout or the e-ink panel; host ratios do not predict
device page-turn percentages. The QA directory records sources, commands and raw
results separately from firmware.

On X4 Pro, compare the prior and new normal firmware with identical SD, book,
font, size, margins and refresh settings. Check Ubuntu UI labels in both landscape
orientations, including selected/inverted labels, clipping near screen edges and
overlays. Check portrait/inverted portrait, rotated side labels, 1-bit SD fonts,
Noto/TrueType 2-bit text, grayscale and strip previews for identical pixels. With
diagnostic logging, compare the existing page render phase times over repeated
turns after warm-up; the full panel refresh can dominate small CPU gains. Record
internal/PSRAM heaps, largest blocks and render-task stack high-water marks across
repeated navigation. Clipped strokes, altered transparent background or bytes
outside a strip are failures. No cache deletion or repagination is required.
