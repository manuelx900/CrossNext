# Flashcards: stress validation — 2026-10-19

## Reproduce and open the installed corpus

Generate with `.venv/Scripts/python.exe tools_local/study/generate_stress_corpus.py`,
then seed with `.venv/Scripts/python.exe tools_local/wasm/seed_stress.py` and rebuild
the browser. The seed helper accepts only `tools_local/wasm/sdcard`; it never
writes a device SD or deletes existing decks. In Flashcards open
**Pruebas_extremas**. Escala contains 100/1,000/10,000/30,000-card decks and a
100-image deck; Integridad contains deliberate failures. A 100-card copy is also
at the root and in a nested folder with a long accented filename. The eight
main synthetic APKGs contain 41,260 exported cards before omissions. Checksums
and case names are in `assets_local/simulator/stress-corpus/manifest.json`.
The pre-existing 31 downloaded decks and prepared anatomy/physics remain.
Synthetic APKGs use schema-11 SQLite and exported card ordinals; they were not
validated by launching Anki Desktop. They supplement, not replace, real decks.

`[units]` in “Define cross-section [units]” is original Nuclear Physics note
1498484253211. It asks for the units and is not an import artifact.

## Evidence and classification

Correcto means the tested content/interaction is faithful and stable; Parcial
means reduction or an explicit documented limit; Fallido means incorrect or
inaccessible content or a hang. Host checks are not physical-device measurements.
Browser verification is recorded separately below; physical-device validation remains pending.

| Area | Result in production-code host tests | Classification |
| --- | --- | --- |
| Basic, accents/entities, reversed | Correct question/answer, reverse ordinal swaps faces; byte content checked | Correcto |
| Long text | 1k/5k/20k, 4k/32k/65,535 literal ampersands, 20k question + 50,004-byte answer retained; a 2k unbroken token wraps; every 50k-character unbroken-text page reconstructs original in both viewport orientations | Correcto |
| Over-limit/empty faces | 70k-byte face and empty question/answer omitted and counted; later valid cards remain | Correcto rejection / Parcial content compatibility |
| Line breaks / malformed HTML | Block/list breaks preserved, redundant blank breaks normalized; unclosed ordinary tags tolerated when parseable | Parcial |
| HTML / tables / code | Lists and entities readable, cells use separators and rows use newlines; pre whitespace retained, sub/sup markers retained. No column grid, merged cells or monospace guarantee | Parcial |
| Math / chemistry | Linear fractions, roots, integrals, sums, matrices/aligned rows, charges/isotopes and mhchem reaction text; no TeX typesetting | Parcial |
| PNG/JPEG/alpha / 10 MiB JPEG | Actual production PNG/miniz and scalar JPEGDEC conversions, output 448×336; transparent PNG accepted; media bytes may be streamed/padded | Correcto for tested files |
| GIF / WebP / missing/broken/external | No usable image; image-only cards omitted and counted. Video alternative text survives, playback absent | Parcial compatibility; explicit rejection |
| 4K / 8K / extreme ratios | Source bounds 2048×3072 reject the 4K/8K and 8192-wide/tall cases | Parcial; no crash |
| Cloze | c1/c2/c3, shared ordinals and hint supported; a hidden image is absent from question and present on answer | Correcto |
| Nested cloze | Two nested-cloze ordinals omitted and counted | Parcial; explicit rejection |
| Image Occlusion / anatomy | Prepared masks in downloaded anatomy/physics; unprepared Enhanced records skipped. Tiny gray leader lines lose detail in one bit | Parcial; device legibility pending |
| CSS / custom templates / type-in / JS | Static first-two-field fallback, no CSS/template execution or type-in interaction. Script/style source removed | Parcial |
| Unicode | UTF-8 bytes preserved. Installed fonts determine glyphs; fixtures do not add CJK fonts; RTL shaping/bidi and color emoji unsupported | Parcial; visual limitations |
| Corrupt / oversized / traversal | Truncated/non-ZIP/300-MiB declared collection rejected; archive `../` members do not create files outside cache; source and sentinel unchanged | Correcto for these cases; not a complete security audit |
| Scale | 100, 1k, 10k, 30k imported; every note loaded; last 30k card exact; 100 distinct image members converted | Correcto on host; device latency/RAM pending |
| Persistence | 500 CardState updates reread correctly, completed reimport refused without changing state; interrupted/short-write imports publish no metadata, retry succeeds; PSRAM failure rejects cleanly | Correcto in modeled HAL; power-loss durability pending |
| Hierarchy / duplicates / long names | Installed root and nested copies with matching IDs exercise separate source caches; root/folder paging navigated in browser; internal Anki subdeck/tag hierarchy not reproduced | Parcial |
| Audio / video / animation | No playback/animation, no content execution; text alternatives only | Parcial |

Host command: `CXX=<host C++ compiler> sh host-tests/flashcards-stress/run.sh`.
It compiles the production importer, SQLite/ZIP/miniz/zstd, PNG and JPEG
converters over an in-memory HAL. JPEGDEC is built with `NO_SIMD` because this
Windows/x64 test's vendor SSE path fails on an otherwise valid baseline JPEG;
the scalar path passes. This does not verify ESP32-S3 SIMD behavior or the
browser's decoder shim. Host heap is not a measurement of S3 memory.

Native run: **43,362 checks passed**. Main deck outcomes:

| Deck | Imported | Omitted |
| --- | ---: | ---: |
| Textos y formatos | 28 | 3 |
| Imágenes límites | 7 | 10 |
| Cloze y plantillas | 10 | 2 |
| Scale 100 / 1,000 / 10,000 / 30,000 | 100 / 1,000 / 10,000 / 30,000 | 0 |
| 100 images | 100 | 0 |

Host times are recorded in
`qa-artifacts/flashcards-stress-2026-10-19/native-stress.log`; they are PC times,
not X4 Pro benchmarks. In that run 30k imported in roughly 0.3 s. Additional
study-suite checks exercise SQLite overflow, CSV boundaries, zstd, cancellation
and durable stores. UI host tests render PHOTO and paginate in 480×800 and
800×480. Browser screenshots and final firmware build results are recorded in
the QA folder when complete.

## Browser interaction evidence

The compiled firmware in the browser imported anatomy's 282 cards with 0 omissions.
The indicator advanced through image preparation: 9 cards / 26 s, then 36 cards / 54 s,
and later completed. This validates visible updates during busy imports, not
just the progress DTO. Screenshots are under the QA `screens/` directory.

The text deck imported 28 cards and explicitly counted 3 omissions. Actual touch
navigation reached the 1000-character answer's 4/4 END, the 20000-character
unbroken question's 65/65, and the 50004-byte answer's 256/256 END. The answer's
question summary returns to the full question. The 256-page run used 255 real
arrow taps; the 65-page run used 64. These are page transitions, not 500 reviews
or panel refresh measurements. The 5k and 20k answer counters read 1/20 and 1/74.

Anatomy image-only question/answer faces open full-screen. Visual verification
also found a Back argument error: passing Color::White to the bool fillRect API
produced black on black. That mechanical argument is corrected to false; the final
recompiled browser shows ATRÁS centered with the arrow. Both rectangle and
arrow return to the same answer; PHOTO is visibly bordered and opens the image.
Cloze c1/c2/c3 and hints were also exercised interactively.

Final native firmware checks passed for X4 Pro, Sticky and PaperMono. The
PaperMono artifact is retained from the shared build after the last source edit;
X4 Pro and Sticky were rebuilt afterward. The export includes concurrent Chess
work; these binaries are validation artifacts, not an isolated release. Nothing
was flashed, pushed or published.

The separate 30k browser import completed with **30,000 imported / 0 omitted**.
The deck overview reports 30,000 cards; opening review shows Question 00000 and
Answer 00000. Screenshots `scale-complete.png`, `scale-deck.png`,
`scale-question.png` and `scale-answer.png` record the actual UI. Intermediate
captures show 3,905 cards / 2m10s and 24,810 cards / 23m53s; the last observed
progress was 28,811 / 31m17s before completion. This background browser run
overlapped builds and is not an S3 latency benchmark or an exact completion time.

The full Unix `scripts_local/check.sh --tests` could not run in this gitless
Windows export: its Python command hit the Store alias, CMake/Ninja are absent,
and Git-based formatting has no root repository. That is not a green full gate.
It also reported a `backgesture` source-audit failure for FlashcardsActivity.
That scanner recognizes only loop/gameLoop/route-prefixed methods
(`host-tests/backgesture/run.sh:122`); it does not follow the direct call from
`FlashcardsActivity::loop()` at line 324 to `handleCustomInput()`, whose first
branch reads Button::Back at line 189, before any tap/readiness gate. Main-agent
inspection therefore identifies a scanner false positive, not an unreachable
Back path. The original failed result is retained; the audit was not changed.
Affected host suites and the sanctioned formatter ran with explicitly selected
tools; native firmware targets and browser are built under the checkout lock.

## Ownership and compatibility

Native v4 has a 524,320-byte import-owned PSRAM workspace, reused per card and
freed with the importer. StudyActivity and History each own one reusable
131,104-byte PSRAM Note buffer; both can coexist when History is pushed.
Note's 8192-byte inline storage remains, and the core never allocates. No
internal-heap fallback or new task/framebuffer is introduced. Decoder buffers,
zstd windows (up to 4 MiB) and existing app allocations add to those explicit
requests; the largest observed explicit test allocation is not total peak RAM.

Legacy v2/v3 and paired/v1 image caches remain readable. Existing completed
caches are not overwritten; to exercise new conversion behavior use a new
filename or an explicitly disposable cache, preserving cards.dat/revlog.dat.
Old firmware/computer tools limited to v3 cannot consume new native v4 content.
Native v4 plain card faces paginate fully. Existing rich vocabulary layouts from
the separate computer conversion route have their own field/font behavior.

The browser HAL now serializes presentation and publishes from its existing
render worker during busy import loops. This is browser-only; desktop SDL keeps
its main-thread presentation, and device display code is unchanged.

## Physical X4 Pro / Sticky verification (not performed here)

1. Back up SD, mazos, cards.dat and revlog.dat. Install a separately reviewed
   firmware build through the user's normal process; no flashing is performed
   by this task. Use copies of stress packages and a new cache/source filename.
2. Import 100 / 1k / 10k / 30k and 100 images. Record elapsed import/open/card
   latency, source and decoded sizes, errors and skipped count. Expect progress
   phases/time to update; repeated unchanged UI without advancing time is a
   failure. Allow slower phases rather than assuming a global percent.
3. With Serial logging, measure free/minimum heap and largest free block plus
   PSRAM before/peak/after import, Study, History, image view and exit. Repeat
   500 review transitions and compare after exit; no monotonically lost memory,
   resets or watchdog exceptions are expected. Explicit allocation failure must
   return an error rather than abort. These measurements are still required.
4. In all four orientations (portrait, inverted portrait, landscape CW/CCW), traverse long question and answer to their END,
   open PHOTO, use rectangular Back and arrow, return to the same face/page.
   No obscured pager, clipped last text, leaked cloze image or wrong face is
   acceptable. Verify rotated touch and logical physical buttons.
5. View prepared anatomy and radiography-like diagrams, then alternate a dark
   image and white card. Judge leader lines, small labels, ghosting and full
   refresh cadence on the panel; simulator screenshots cannot certify this.
6. On disposable copies, restart during import and after a recorded review,
   simulate nearly full/unavailable SD, and recover by reopening. Compare card
   IDs and review log against backups. Test low battery only through the normal
   device procedure. A modeled short write is not proof of power-cut atomicity;
   partial cards.dat writes remain an explicit durability risk to assess.

Official reference: [Anki math](https://docs.ankiweb.net/manual/math),
[template generation](https://docs.ankiweb.net/manual/templates/generation),
[styling](https://docs.ankiweb.net/manual/templates/styling). Anki's MathJax and
pre-generated LaTeX images are different paths; this firmware supports neither
full engine, only bounded text fallback and supported raster media.
