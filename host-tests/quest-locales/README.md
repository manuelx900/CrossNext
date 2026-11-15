# Quest locale layout audit

Run from this worktree:

```sh
python host-tests/quest-locales/run.py
```

On Windows, use the existing Emscripten installation:

```sh
python host-tests/quest-locales/run.py --emsdk /absolute/path/to/emsdk
```

This is a host measurement and bitmap preview, not a hardware screenshot. It uses the
firmware's EpdFont, EpdFontFamily, MiniBidi, built-in Ubuntu/Noto UI glyphs, and the
nine microSD Interface fonts. It loads each current YAML catalogue by the same key
order as I18n and uses English for missing entries, as the firmware does.

The cases cover every Quest view, all gear details, every hunt, battle states,
active hunt decisions, every inventory/shop page, purchase confirmations, victory
records with dates and finished books, rank/level milestones, save errors,
all current Saga/Path narrative pages and the prologue,
in portrait and landscape, using the configured X4 Pro and Sticky bezel insets. The trace checks wrapping, screen bounds and glyph
clipping and missing shaped glyphs. Representative previews omit external creature art; this audit focuses
on text geometry. This is separate from the existing study-suite interaction and asset tests.

Use `--board x4pro` or `--board sticky` to restrict a run; the default checks both.

`check_fonts.py` checks both faces at 8/10/12 pt, including SC/TC regional faces,
against all catalogue characters, keyboard layers, long-press alternatives,
Pinyin candidates and Wordle alphabets. Reports are written below
`.cache/locale-ui/`.

This checks the installed catalogue snapshot. It does not certify linguistic
quality or layouts of translations that have not yet been written. Fonts must be
installed under `/languages/fonts/` on the card; their glyphs are not embedded
into the firmware binary.
