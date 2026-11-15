# Localized UI checks

These host checks use the firmware's EpdFont data and the generated SD interface
fonts. Generate I18n with the same built-in language list as the device first:

```sh
python scripts/gen_i18n.py --strip-unused --builtin-langs en,es
python tools_local/language_packs.py bundle .cache/locale-ui/languages
python host-tests/locale-ui/run.py --emsdk .venv/emsdk --output .cache/locale-ui/pet
python host-tests/locale-ui/cards.py --emsdk .venv/emsdk --output .cache/locale-ui/cards
python host-tests/locale-ui/assets.py --packs .cache/locale-ui/languages --output .cache/locale-ui/web
python host-tests/locale-ui/web.py --scripts .cache/locale-ui/web --output .cache/locale-ui/browser
```

The browser check accepts `--browser /path/to/chrome` to use an installed browser.
The asset check uses the local `.venv/emsdk`; the PET runner accepts another SDK.
Run Emscripten suites sequentially when they share a cache. Native `check.sh`
and `sim-shot.sh` need the tools listed in the workspace build instructions.

PET covers 39 catalogues, portrait/landscape, three species and five growth
stages, including Cold/Malaise health captions, plus help/catalogue pages and all three minigames' phases. Its real-font
profile uses Ubuntu 12/10, Noto Sans 8 and the matching SD script fallbacks.
It checks complete text, missing glyphs and screen bounds. Text intentionally
masked by a drawer's viewport is not counted as an unexpected clip. Captured
PBMs use fallback artwork and are host renders, not device screenshots. Other
font profiles, artwork overlap, all lifecycle states and every possible user
name are outside this matrix.

The asset check loads each generated external pack through the real C++ loader,
compares active lookups to the YAML fixtures, generates `/ui.js` and checks its
JavaScript syntax. Ook intentionally falls back for untranslated strings. The
browser check covers English, Spanish, German and Arabic in two viewports,
including RTL, HTML escaping, reordered named arguments, complete font-upload
messages and unchanged user note contents/filenames. Upload requests in this
browser test are intercepted fixtures; no reader or user file is modified.

The PET runner also executes `test_gfx_fitter.cpp` through the production SDK
`GfxRendererTarget`, using real font metrics and a recording renderer stub.
It requires the complete Intercept credits to survive explicit newlines without
delegating paragraphs to the backend's lossy wrapper. This is an adapter
contract check, not a full GfxRenderer simulator or a hardware screenshot.

The same runner executes `counts.cpp` against the 39 generated fixtures. It uses
real C++ printf to check seven Study counters with singular, plural and large
counts, plus the card statistics, timeline, date/write warnings, ready-decks notice
and sync bodies.
It rejects text cut by the existing UTF-8 byte buffers and English plural suffixes
displayed in external natural languages. The real-font probe also checks the
failed-review warning as a complete single line in the small font. The documented contextual auditor also
checks these contracts while reading current buffer capacities from source:

```sh
python tools_local/i18n/audit_context.py --output .cache/locale-ui/audit --packs .cache/locale-ui/languages
```

This pack export avoids copying the unchanged Wordle word banks. Existing fonts
and keyboard resources still need to be installed for the visual checks.

`centered.cpp` is a real-font probe fed by a call-site-derived `centered.inc`
fixture. Its bounds must be kept aligned with the corresponding system screens;
it is not a simulator of the entire activity. The older `i18nwidth` survey
cannot measure scripts absent from its built-in font and must not certify them.

Keep `test/language_pack/test_language_packs.py` green for all source catalogues.
Complete catalogues and fitting labels do not certify native-language wording.

Study's additional real-builder matrix runs separately:

```sh
python host-tests/locale-ui/run_study.py --output .cache/locale-ui/study
python tools_local/i18n/catalogue_signals.py --output .cache/locale-ui/editorial
```

It renders 39 catalogues, both orientations, four interface faces and six deck/sync
states. Font bindings follow ToyboxFonts (Pixel, Classic, Noto Sans, Noto Serif),
including the small-slot SD fallback. It fails on clipping/missing glyphs matched
to the current correction ledger and on unclassified diagnostics. Unchanged
strings drawn by the same builders remain in `legacy-issues.json`; use `--strict`
to fail on those too. A green regression result does **not** certify the complete
Study screen. The full `study-issues.jsonl` is always retained.

`catalogue_signals.py` scans every source value for English fragments, numeral or
identifier differences, and narrative-name inconsistencies. These are review
signals, not confirmed errors; native inflection, time units, product names and
numeric punctuation can be valid. The contextual regression ledger is separate
from this editorial queue. Never declare all phrases accurate from a clean
structural audit or remove a useful signal merely to make its count zero.
