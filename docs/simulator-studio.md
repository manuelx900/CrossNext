# Screenshot studio

Double-click `Start-Simulator.cmd` in the source checkout to build and open the
current browser simulator. The prebuilt `CrossNext-Simulator-2.0.0` package opens
the same studio without PlatformIO or Emscripten: extract it, install Python
3.12 or newer if needed, and double-click its launcher. Chrome and Edge support
the threaded browser build. Keep the local console open while using it. The
prebuilt launchers use `http://127.0.0.1:8767/`; pass `--port 8768` if that port
is occupied. A running server from an older studio version is not reused.

The firmware starts in English, on Bookshelf Home. **Firmware language** selects
any of the 39 catalogues. **Appearance** chooses the four Home themes.
**Open on startup** lists all 15 apps and 16 games directly from the firmware
registry. You can also navigate through the normal firmware menus.

Every selector reloads the simulator and restores its synthetic card. This
avoids cached labels from the previous language and makes screenshots repeatable;
session interactions are discarded. Books, character names, notes and other
sample content retain their own language. These are separate from translated
interface labels.

**Save PNG** exports only the screen, at 480 × 800 or 800 × 480 according to
orientation, without toolbar, scaling or a device frame. **Clean view** hides
the controls for screen recordings; Escape or **Show controls** restores them.
Touch, hold and swipe the panel as on the reader. The browser's **Back gesture**
button performs the left-edge swipe, rather than emulating an extra hardware key.

The virtual card includes books with covers, 120 days of synthetic reading,
habits, task lists, notes, quotes, flashcards and vocabulary. Pet has three
species at five ages, a furnished room, supplies, memories and three previous
generations. Reading Quest has three progress levels and both heroes, including
equipment, campaign progress and coins; its screen selector opens the collection,
shop, path, hunts and other real app screens. Focus has a project and tasks; the
counter starts at 128. Eight existing game/app save formats are regenerated and
decoded by current production codecs. Chess and Solitaire retain their sample
saves. Games with no resumable board start on their normal menu and remain
playable; choosing one starts a real game, not a staged image.

The simulator clock advances from the displayed demonstration date at 16:30 UTC.
It starts there on every reload. Reading sessions precede that time, and Pet's
save and shared journal agree, so storing the package for several days does not
age the pet before you open it. Actual firmware rules, artwork and rendering
remain in use. This simulator does not model e-ink refresh latency or measure
device performance.

The server binds only to `127.0.0.1`; nothing is published. All writes are to the
browser's disposable in-memory card. No real device SD card is populated.
Book attribution and rights notices remain on that card. See
[showcase sources](simulator-showcase.md).

## Maintaining the fixtures

`seed_resources.py` builds the current app artwork and calls `seed_showcase.py`
and `prepare_promotion.py`. The latter compiles host-only save generators against
production Pet, Quest, Focus, counter and game codecs. It exports the current
language packs, keyboard resources and interface fonts using the existing
language exporter, filtering to the firmware's active keys. Wordle word banks are
included in the package and the selected language is loaded onto the virtual card
at startup, so opening Home does not preload every vocabulary into browser memory.
The local server serves the banks with lossless Brotli transfer compression.
`site/showcase/catalog.json` records the scenario saves, registry and seed date.
No hand-written alternative firmware UI or production device setting is added.

The browser shim's `crossplay_start` is called after card overrides are installed,
before the first firmware singleton can read settings. Older browser artifacts
retain their existing automatic-start behavior. Rebuild the current browser
artifact when changing this shim. The studio changes languages by reloading the
whole instance; it never mutates firmware singletons from the browser thread.

After building, `python tools_local/wasm/package_promotion.py` creates the
prebuilt folder and ZIP under `outputs/`. Its launcher verifies packaged file
hashes and serves them without a source rebuild. The package requires only
Python's standard library at runtime. Development builds still use the existing
source and artifact freshness checks.

Run `python host-tests/simulatorweb/test_simulator_web.py` for server and bundle
integrity checks. Run `node host-tests/simulatorweb/test_studio.cjs URL OUTPUT`
against a running studio for real-browser boot, locale switching and PNG export;
`CROSSNEXT_PLAYWRIGHT` can name an installed Playwright module.
