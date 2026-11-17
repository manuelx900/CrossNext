# Display uploads and cover-notice help

The UC8279 X4 driver sends an unmodified visible plane in chunks of at most
32,000 bytes instead of copying and sending every visible row separately.
The source buffer is borrowed during the existing synchronous transaction;
padding gates, pixel values and transaction boundaries stay the same. Inverted
or mirrored transfers keep the established row path. No framebuffer, task,
clock change or refresh waveform is added. This adapts
[CrossNext PR31](https://github.com/manuelx900/CrossNext/pull/31) by Snouzy.

The benefit is reduced per-call transfer overhead. Wire-byte equivalence is
tested; physical page-turn timings and radio/memory peaks remain device tests.
The change lives in the existing SDK driver because UI code does not own panel
plane transport. The existing HAL/display entry points remain unchanged.

## One-pass experiment

[PR30](https://github.com/manuelx900/CrossNext/pull/30), inspected at
`259e4aa322ea1404dd281bb8180f7d0ac56dc52c`, was evaluated in an isolated host
experiment. Its actual `X4ProTextAa` composer maps distinct AA mask codes 01
and 10 to the same output gray and requests 624,000 bytes of PSRAM. This fails
the requested identical-quality condition. The candidate, its buffers, scope
and speed option are therefore not included in the firmware. The original
two-gray text path and existing display clock remain in use.

## Why enable the cover notice?

Appearance > Cover loading notice has a compact button at the bottom right.
It explains that Home appears sooner because covers load on that screen.
The setting still applies separately to each of the four Home appearances.
The four paragraphs describe the enabled behavior, temporary covers, disabled
behavior and the advantage of a faster return from a book or another section.
The help describes existing behavior: an enabled choice enters the Home shell
before preparing content; a disabled choice prepares content before painting.
It does not promise faster decoding or an immediate response during cover work.

The explanation is a read-only view in the same settings activity. Back returns
to the choices, keeping their values and selection. Reading it writes no setting,
starts no task and creates no child activity. The footer reserves measured space
for its translated label; borders use the current interface theme's square or
rounded corners. The regular checkbox path retains its one-tap repaint behavior.
The explanation keeps the selected menu font when it fits. In a constrained
view it uses the existing compact interface face for this text only, then the
existing smallest face if necessary; EPUB fonts and saved preferences do not
change. Footer drawing uses themed primitives and one hit target rather than
another large button descriptor. Native text wrapping still uses the renderer's
existing temporary string/vector work; no retained content buffer is added.

The button and four complete explanatory paragraphs are supplied in 39 catalogue sources. English and Spanish are
built into the firmware; 37 external `.cplang` packs are regenerated for microSD.
Ook remains fictional and partial. The resource update ZIP replaces language
packs only: merge it into the card root and retain installed fonts, keyboards,
word banks, apps, dictionaries, books and `.crosspoint` data. Restart afterward.
The complete resource builder reads the same updated sources for future bundles.

## Checks and hardware steps

`host-tests/display-upload/run.py` checks all plane bytes and chunk boundaries
for row reversal, mirroring, inversion and padding, then runs the complete SDK
Pro driver/facade suites in single/dual-buffer host configurations. This does
not enable dual buffering in the firmware. The direct path uses two visible
data calls; transformed paths keep their established behavior.

`host-tests/cover-notice-help/run.py` exercises production geometry, existing
FreeInkUI drawing, translated strings and production open/Back callbacks. Raster
captures use seeded choices and metric checks cover all four menu-font choices,
including the formerly overflowing Indonesian/Serif landscape case. Full native simulator execution is unavailable on
this Windows host. Geometry coverage does not certify native-script glyph
rendering, shaping or physical touch/display behavior.

On the X4 Pro, inspect EPUB text with AA enabled, illustrations, dark mode,
menus and all four orientations. Compare with the previous BIN using the same
book and card. Look for missing/shifted rows or new artifacts. Check Bluetooth
page turns and Wi-Fi transfers, plus internal/PSRAM free bytes and largest blocks
over repeated transitions. Open the help from every appearance, then Back and
toggle each checkbox once. Check long translations and a non-Latin language
after updating packs. Source preservation and host timing are not physical
radio, image-quality or latency measurements.
