# MicroSD resource package

Install the firmware for your device, then extract `CrossNext-Resources.zip` and
copy its **contents** to the root of the microSD card. Replace the bundled resource folders; keep books and saves.
This package does not contain or replace an `images` folder.
Do not add an extra `resources` directory around them. Power off before removing
the card and restart the reader after copying.

PET and Reading Adventure load artwork only from `/apps/pet-v2/` and
`/apps/reading-adventure-v2/`. The unversioned folders from older releases are
ignored. Install the matching ZIP; renaming an old folder is insufficient:
all six packs must use the current grayscale format and expected image counts.
Missing, mixed or incompatible contents show the existing installation notice.
No directory scans, new cache or background task are needed for this check.

Saved progress stays at its existing paths under `/.crosspoint/`, including
`/.crosspoint/pet-v2/` and `/.crosspoint/reading-adventure-v2/`. These are save
directories, separate from the artwork under `/apps/`. Never replace or remove
`/.crosspoint/` when installing resources. Old artwork folders may stay on the
card; this firmware does not read them.

The card layout is:

```
/apps/reading-adventure-v2/graphics.bin
/apps/reading-adventure-v2/manifest.json
/apps/reading-adventure-v2/story.bin
/apps/reading-adventure-v2/story-manifest.json
/apps/reading-adventure-v2/rewards.bin
/apps/reading-adventure-v2/guardians.bin
/apps/reading-adventure-v2/bestiary-outfits.bin
/apps/reading-adventure-v2/expansion-manifest.json
/apps/reading-adventure-v2/celebrations.bin
/apps/pet-v2/graphics.bin
/apps/pet-v2/home.bin
/apps/pet-v2/habitat.bin
/apps/pet-v2/breathing.bin
/apps/pet-v2/farewell.bin
/apps/pet-v2/collection.bin
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
Replace `/apps/reading-adventure-v2/` with the folder from the matching resource ZIP
and reopen the app. The old unversioned `moments.bin` is no longer distributed
or used. Installing artwork does not start a new save generation.
If the card becomes unreadable while drawing, the same notice
appears and further game actions are blocked.

Reading Adventure progress uses `/.crosspoint/reading-adventure-v2/reading-quest-v10.dat`; PET
uses `/.crosspoint/pet-v2/`. These redesigned apps start a separate [save generation](apps/save-generation-v2.md),
leaving earlier game files intact without importing them. Intercept retains its existing store.
Binary profile formats and equipment identifiers are unchanged. The ZIP contains no
`.crosspoint` directory or saved profiles. Updating the package replaces the distributed resource folders;
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

`tools_local/quest/build_resource_pack.py` exports the canonical smooth artwork
under `assets_local/quest/redesign/masters/` and `assets_local/quest/campaign/masters/` as 1,029 CNART003 grayscale records.
The standalone exporter is `tools_local/quest/build_gray_art.py`. Backgrounds
use two 240-by-320 halves; figures and companions retain separate opaque masks.
Legacy encoded headers remain historical inputs, not the redesigned runtime art.

CNART003 uses a 24-byte little-endian header: eight magic/version bytes, sprite
count, total bytes, CRC32 of index plus pixels, and index CRC32. Each 16-byte
index entry contains offset (u32), width (u16), height (u16), pixel byte count
(u32) and image CRC32 (u32). Records are contiguous, row-padded, MSB-first,
two-bit ink coverage. The shared decoder also accepts older CNART001/2 packs
for compatible callers; Reading Adventure requires its new 1,029-record layout.
The manifest records SHA-256 hashes for distribution checks.

The activity owns one read-only `HalFile`, a 70-byte maximum row buffer and,
when available, one temporary PSRAM allocation for index plus 4 KiB read-ahead.
For 1,029 records this uses 20,560 bytes; the supported 4096-record ceiling uses
69,632 bytes. Without PSRAM or on allocation failure it reuses a 512-byte
buffer and validates index entries in sequential batches. The index CRC and
all record bounds are checked on entry; image CRCs are checked before drawing.
Only displayed images stream afterward. The cache and handle are released on
failure and activity exit. There is no full-pack RAM allocation, extra
framebuffer, worker task or network connection. Entry and drawing times still
require measurement on the physical reader and card.

## Customization

Users can replace resources: there is no DRM or secret signature. CRC32 detects
invalid data; it accepts a compatible modified pack with a recalculated checksum.
Graphics live inside `graphics.bin`, so they are not individual PNG files on SD.
Edit the project's art inputs and regenerate the pack, preserving the 1,029 records,
group order, meanings and ink/mask pairs. See the ZIP's `CUSTOMIZATION.txt` and
[art generation](apps/quest-gear.md). Fonts and StarDict folders can be added
directly using their supported layouts. Preserve third-party licenses.

## Device checks

With an existing save backed up, open the app without resources: instructions and
Back must work, with the profile file unchanged. Copy the ZIP contents, restart,
and check that level, coins and equipment remain. Compare the hero, pets, backgrounds,
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


### Reading Adventure progress backup

Copy `/Backups/AventuraLectora-v2/partida-v10.rqu` off the SD card before updating or
changing cards. Aventura Lectora automatically verifies and updates this copy
when saving and exposes Create/Restore under its Settings. To move progress,
copy the complete `Backups/AventuraLectora-v2` folder to the new card alongside
current artwork resources. The app recovers it when its primary save is missing.
The copy includes equipment, favorites, victory records and adventure progress;
books and the general reading-statistics journal are separate. Keep the original
card until the restored adventure has been checked. Downgrades before RQU9 cannot
read the new8192-byte record; retain the original earlier-version save too.
