# MicroSD resource package

Install the firmware for your device, then extract `CrossNext-Resources.zip` and
copy its **contents** to the root of the microSD card. Merge existing folders.
Do not add an extra `resources` directory around them. Power off before removing
the card and restart the reader after copying.

The card layout is:

```
/apps/reading-adventure/graphics.bin
/apps/reading-adventure/manifest.json
/apps/pet/graphics.bin
/apps/pet/home.bin
/apps/pet/habitat.bin
/apps/pet/breathing.bin
/apps/pet/farewell.bin
/apps/pet/collection.bin
/apps/intercept/graphics.bin
/apps/intercept/manifest.json
/fonts/Bitter/...
/fonts/Lexend Deca/...
/fonts/Literata/...
/fonts/Atkinson Hyperlegible/...
/fonts/Merriweather/...
/fonts/EB Garamond/...
/fonts/PT Serif/...
/fonts/Inter/...
/dictionaries/English Simple/...
/dictionaries/Spanish Wiktionary/...
/dictionaries/WikDict EN-ES/...
/dictionaries/WikDict ES-EN/...
/dictionaries/WikDict EN-FR/...
/dictionaries/WikDict FR-EN/...
/dictionaries/WikDict EN-DE/...
/dictionaries/WikDict DE-EN/...
/dictionaries/WikDict EN-PT/...
/dictionaries/WikDict PT-EN/...
/licenses/...
/THIRD-PARTY-NOTICES.txt
/SOURCES.json
/CUSTOMIZATION.txt
```

The package includes Reading Adventure, PET and The Intercept. Each requires its
complete compatible resource folder. If files are missing or unusable, the app
shows installation instructions and its folder path instead of starting. Back
returns to the launch location. See [PET](apps/pet.md) and
[The Intercept](apps/intercept.md) for their required files and validation flow.
The fonts and dictionaries are optional and use the reader's existing discovery
paths. Font licenses are included in their family directories. Dictionary sources,
authors and CC BY-SA 4.0 attribution are retained in the StarDict `.ifo` files,
with explicit attribution and full license files in each dictionary folder.
The optional xxyzz CSS source files retain GPL-3.0-or-later notices as well.
Keep the notices when redistributing; these are copyrighted, openly licensed
works. See [resource sources and selection](resource-sources.md).

For the Spanish interface, installed defaults are **Spanish Wiktionary** for
definitions and **WikDict EN-ES** for translations. For English, they are
**English Simple** (English definitions in simple English) and **WikDict ES-EN**.
The firmware fills unassigned automatic slots at boot and when selecting the
interface language. An existing dictionary name is never replaced, even if an
older version left its automatic flag enabled. Each role is handled independently.
Manual choices, including None, survive reboots and language changes; previously
empty legacy selections adopt the defaults. Missing dictionaries are never
automatically selected for an unassigned slot. A missing assigned folder keeps
its saved name so temporarily removing resources does not erase the choice.
The old `Espanol Wikcionario` folder is accepted as a fallback for Spanish.
Other interface languages receive no new automatic monolingual default.

Reading Adventure checks the artwork package before loading or changing its
profile. Missing, incompatible, truncated or checksum-invalid resources display
instructions instead of starting the game. Back returns to the launch location.
Replace `/apps/reading-adventure/` with the folder from the matching resource ZIP
and reopen the app. If the card becomes unreadable while drawing, the same notice
appears and further game actions are blocked.

Reading Adventure progress remains in `/.crosspoint/reading-quest-v5.dat`; PET
and Intercept also retain their existing stores under `/.crosspoint/`. Existing older-profile
migrations and equipment identifiers are unchanged. The ZIP contains no
`.crosspoint` directory or saved profiles. Updating the package merges resources;
it does not require formatting the card or deleting books, settings or saves.

## Building the package

Run from the project root:

```
python tools_local/build_resources.py
```

This builds a fresh staging tree and publishes `resources/` and
`dist/CrossNext-Resources.zip`. Rebuilding does not retain removed inputs.
The generator replaces only its unmodified previous output; preserve manually
edited or additional files elsewhere before rebuilding.
The ZIP contains the staging tree's contents, not its parent directory. The default
dictionary sources are the existing StarDict collection in
`tools_local/wasm/sdcard/dictionaries` and the six additions in
`assets_local/sd-resources/dictionaries`; `--dictionaries PATH` replaces both
with a custom collection. The additions are not preloaded into the browser
simulator. Additional fonts can be installed using the existing
[SD font layout](sd-card-fonts.md). Regenerate and validate the package whenever
changing its inputs. Never place private files, user books or saves in `resources/`.

`tools_local/quest/build_resource_pack.py` converts the canonical compressed
artwork under `assets_local/quest/encoded/` into 900 lossless BW1 ink/mask records.
The art generator writes its intermediate headers there rather than in firmware
sources. These headers are packaging inputs and are not compiled into firmware.

CNART001 uses a 24-byte little-endian header: eight magic/version bytes, sprite
count, total bytes, CRC32 of the index and pixels, and a zero reserved word.
Each 12-byte index entry contains offset (u32), width (u16), height (u16), and
pixel byte count (u32). Records are contiguous and row-padded, MSB-first BW1.
Changing sprite order or meaning requires a new format identifier. The manifest
also records SHA-256 hashes for distribution checks.

The activity owns one read-only `HalFile`, a 35-byte row buffer and, when
available, one temporary PSRAM allocation for the index and 4 KiB read-ahead.
For the 900-sprite package this uses 14,896 bytes, with a 53,248-byte maximum
for the supported 4,096 records. Without PSRAM or on allocation failure it
reuses a 512-byte buffer and validates index entries in sequential batches.
The complete CRC and all record bounds are still checked on every entry.
Only displayed sprites are streamed afterward, with unchanged pixels and scaling.
The cache and handle are released on failure and activity exit. There is no
full-pack RAM allocation, extra framebuffer, worker task or network connection.
Entry and drawing times must still be measured on the physical reader and card.

## Customization

Users can replace resources: there is no DRM or secret signature. CRC32 detects
invalid data; it accepts a compatible modified pack with a recalculated checksum.
Graphics live inside `graphics.bin`, so they are not individual PNG files on SD.
Edit the project's art inputs and regenerate the pack, preserving the 900 IDs,
group order, meanings and ink/mask pairs. See the ZIP's `CUSTOMIZATION.txt` and
[art generation](apps/quest-gear.md). Fonts and StarDict folders can be added
directly using their supported layouts. Preserve third-party licenses.

## Device checks

With an existing save backed up, open the app without resources: instructions and
Back must work, with the profile file unchanged. Copy the ZIP contents, restart,
and check that level, coins and equipment remain. Compare the hero, pets, auras,
shop and inventory in all orientations. Try a truncated pack and restore the good
copy. Repeat entering/leaving the app while monitoring internal heap, PSRAM,
largest free blocks and render stack high-water marks. No card should be removed
from a powered device to perform these checks.

With resources installed, boot with English and Spanish interface settings and
check the corresponding Define/Translate selections and a real word lookup.
Select another dictionary or None manually, reboot and change language: that
role must retain the manual choice. Test one missing default folder and reinstall
it before rebooting. Inspect all eight font families in the reader, including
bold, italic and accented French, German, Portuguese and Spanish text.
