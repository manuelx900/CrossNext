# CrossNext screenshot demonstration

The browser uses the real firmware with a disposable generated SD card. Its
reading history, habit check-ins, task lists and game progress are synthetic,
intended for screenshots and exploration. Reloading the local simulator resets
interactions to these examples. No device card is seeded. The seed uses the host local calendar day, matching the browser on that computer. Starting the launcher on a new day refreshes its fixtures; an already running tab continues until restarted.

The generated card contains thirteen books (seven added classics), ten recent
books, 120 days of reading sessions, eight habits, 24 task lists including the
existing examples, 24 extra text documents, existing flashcard/vocabulary/quote
examples and saved progress in Chess Problems, Picross, Carnivoro, Murdle,
Wordle, 2048, The Intercept and Pomodoro. The generated showcase keeps the English/Spanish dictionaries and excludes extreme/corrupted QA decks; original source assets remain available for development.
The exact generated counts and date
are recorded in `SHOWCASE.json` on the browser card.

Book sources and SHA-256 hashes are recorded in
`assets_local/simulator/showcase-books/manifest.json`. Frankenstein, Sherlock
Holmes, Moby Dick, Pride and Prejudice, Dracula, The Time Machine and Don
Quijote were downloaded from their Project Gutenberg pages, which mark these
editions as public domain in the USA. Their original EPUB notices are retained.
The other five short original demonstration books and Alice remain available.
`SHOWCASE-SOURCES.md` on the card repeats the source links.

Normal simulator builds copy verified local assets; they do not download books
or run a native compiler. To download missing book assets, run
`python tools_local/wasm/fetch_showcase_books.py`. Existing EPUBs are retained; move a particular EPUB aside before deliberately replacing its edition. To rebuild the eight save
fixtures, run `python tools_local/wasm/build_showcase_games.py` with a C++17
compiler available, or pass `--compiler /path/to/zig c++`. Each generated save
is decoded by the firmware's own decoder before packaging. Rebuilding the
browser calls `seed_resources.prepare()`, which verifies source hashes and seeds
only `.pio/build/wasm/resource-card` after checking its generation marker.

For clean screenshots, open the Library cover view, Reading Stats, Habits,
Tasks, a puzzle campaign or a saved game. Statistics show five books per page
in portrait and three in landscape, with minutes above their unit. The partial
history warning banner is hidden; invalid journal records remain skipped.

## Preparing a GitHub upload

Build device targets through `scripts_local/check.sh`, which owns the shared
firmware build lock. With `CHECK_BUILD_RELEASE_ENVS=1`, that script selects the
release configurations. The output must include both `firmware.bin` and
`firmware.factory.bin` for each model. Then run
`python tools_local/package_crossnext.py` to package existing images. This
packager does not start builds: it selects the newest complete application/factory
pair under the standard PlatformIO, local release cache or staged release folders.
The distribution copies are `CrossNext-X4PRO-Firmware-v1.0.0.bin` and
`CrossNext-X4PRO-Firmware-v1.0.0-factory.bin`, with checksums and installation
offsets in `dist/CrossNext`.
PlatformIO and the existing OTA protocol retain their required internal names.

Run `python tools_local/export_github.py` to create `dist/CrossNext-source.zip`
and its file/hash manifest. The export respects project ignore rules and omits
local environments, build caches, QA captures, generated SD copies, previous
submission scratch files, Wi-Fi/credential files and private key containers.
It preserves source, tests, required resources, licenses and upstream credits.
Local working files are preserved so an omitted file can be recovered.

The X4 Pro package is `CrossNext-X4PRO-Firmware-v1.0.0.zip`, containing the
application/factory images, checksums, licences and installation notes.
`CrossNext-Resources.zip` is generated separately with
`python tools_local/build_resources.py`. Firmware/resources ZIPs are release
assets and remain outside the source repository's `dist/` history. [README.es.md](../README.es.md) provides
Spanish installation guidance and [the fork summary](crossnext-changes.md)
distinguishes inherited work from CrossNext changes.
