# Wake and Files latency investigation

A deep-sleep wake restarts the MCU. Storage, settings, language and display must
be restored before the first usable screen. `main.cpp::setupDisplayAndFonts`
also initialises reader fonts before routing Home or a resumed book. The wake
button verification is a short debounce, not an added long-press delay. The
intentional Home single/double-tap timing is unchanged. No panel waveform,
framebuffer, reader page cache, font rasterisation or radio policy is altered.

## Eliminated work

`SdCardFontRegistry::scanDirectory` used to inspect every vector font face while
discovering installed families. It now discovers names first and inspects each
family once when requested through `findFamily`; `getFamilies` resolves all
remaining metadata for a font selector. The active reader family remains prepared during startup. The later
[wake refinement](wake-stats-pages.md) avoids loading unused bundled sizes and
language faces before the first screen. Their coverage is resolved before use. Weight/italic selection still uses actual face
metadata. Opening the font selector after wake may perform the remaining
inspections. Unrequested vector families retain only their name/root, without
per-face filename/role arrays. The requested directory is reread once before
inspection; its selected faces then stay cached. Discovery after an installation
discards those lazy results. This avoids retaining extra candidate faces in RAM.

`HalFile::openNextFile(entry, error)` reuses one opaque native handle instead of
allocating/freeing one for every directory entry. The SDK's `openNext` reuses its
in-place file storage. HAL retains the storage mutex around each operation and
releases it between entries; no whole-folder lock competes with Wi-Fi. EOF,
read failure and allocation failure remain distinct. The one existing handle
allocation is bounded by `sizeof(HalFile::Impl)` and released by RAII. A static
shared handle would be unsafe across traversals; the opaque SDK object cannot
live on a caller's stack without breaking the HAL boundary.

Files keeps its two-pass exact-capacity list allocation, hidden-folder markers,
formats and sort order. Both passes reuse that handle. Entry also reuses the
already-open initial directory instead of opening it again. No resident folder
cache or growing journal index is introduced. Files uses Home's battery icon
and percentage geometry/rendering in Framed/Carousel; rounded themes keep their
existing header style.

## Evidence and limits

`host-tests/wake-files/run.py` compiles the production HAL enumeration method and
font registry. The fixture enumerates 1,000 entries using one handle allocation.
For 80 families/480 faces, discovery performs no face inspection; selecting one
family inspects its six faces once and preserves all four style roles. These
are work counts, not measured device milliseconds. UI tests compare Home/Files
battery pixels across both layouts/orientations and charging states.

On X4 Pro, compare wake to a usable Home before/after using the same microSD,
theme, fonts and sleep screen. Repeat with a resumed EPUB and active Bluetooth.
Test Files folders with few/many entries, Unicode, hidden markers and sorting,
and entering with a remembered book path. Repeat all orientations. Monitor
internal heap, largest free block and PSRAM across repeated transitions; check
Wi-Fi transfer and Bluetooth reconnection afterward. Initial font loading,
SD latency, cover decoding and the physical e-ink refresh can still take time;
physical timing and dynamic memory peaks require this hardware run.
