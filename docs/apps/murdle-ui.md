# Murdle reference redesign

The local Murdle UI follows the supplied references. The catalogue has 400
cases, with 100 per difficulty and a visible difficulty selector. Both orientations
use a 4×4 grid, a shared page control and seven pages per difficulty. Solved cases
have a check and filled status strip; the open case is distinct from unsolved
cases. The first 50 seeds per difficulty are the original cases, with progress
migrated to their new positions.

## Playing and lives

Each case starts with3 lives. Reading/reviewing clues, marking a table and undoing a mark cost none. A complete incorrect accusation consumes exactly1 life. The result screen offers further investigation or reopening the accusation while lives remain. At zero lives it offers retry, a read-only solution and the catalogue. Neither continuing from Menu nor reopening the same case bypasses a failed case. Retry resets notes/attempts directly on the loaded puzzle, preserving both logical and cosmetic identity without a generator allocation; viewing its solution does not mark it solved. Correct accusations preserve remaining lives, complete campaign progress and allow the next case or read-only deduction notes.

MurdleSession.h owns the bounded life transition. Activity owns navigation, picks and persistence. The screen builders consume models and return the exact geometry used for table hit testing. Table pages show one pair of categories. Header and row illustrations use the
same portraits, weapons and places as the dossier, alongside full names.
Accusation choices and the selected summary use those same images. The life-cost
notice has bottom padding; English and Spanish labels fit without ellipses in
both orientations. All3 pairs at3categories and6 pairs at4categories remain available, including motives. Impossible cases explicitly explain that the murderer lies while everyone else tells the truth, beside their clues and on the tutorial's difficulty page. Clue/table/dossier pages have separate positions. Dossier tabs expose people, weapons, places and motives when present. Table crosses and check marks are centered in square bounds, so rectangular
cells do not stretch them. Undo restores one prior entered table mark; it is session-local and does not undo an accusation.

## People and illustrations

The supplied sheet contains30 portraits in explicit pools: female portraits are
Person10, Person15–18 and Person20–29; all other IDs are male. Portrait IDs do not
encode gender by a numeric range. The presentation name pool likewise contains15 male and15 female names. A saved presentation seed selects distinct names and a separate gender-matched portrait permutation. They remain stable on reopen/retry and may vary between cases. Corrected portrait
classification applies to existing saves too: only an incorrectly classified
Person10/Person19 selection changes; names, puzzle seeds and progress stay intact. This cosmetic identity layer does not regenerate or alter the case's logical assignments, attributes, clue masks or fingerprint. Text in clues, accusation and dossier uses the same identity function. Listed textual traits are the authoritative data for deduction; portrait line art is decorative.

The three-page tutorial uses the shared previous/page-count/next control.
Its last page explains all four difficulties, category sizes, motives and the
Impossible statement rule. The notebook page concentrates on table marks and
notes. There is no Start case button or automatic case launch from tutorial
confirmation; Back returns to the menu. Its cross-reference
illustration is assembled from the existing sprites and native link marks, without
the caption embedded in the old image. The wrong-accusation screen replaces the
source bitmap's clipped question mark with a complete native Help mark. No person
portrait bitmap is changed.

Old saves use their original names and derive portraits from the original case seed. The supplied menu, help and outcome drawings are cropped into assets_local/murdle and encoded by tools_local/murdle_art/generate.py as constexpr1bit flash data. There is no runtime image decoding, texture cache, framebuffer or task added. The existing generator scratch allocation is unchanged and remains released after generation. New session working data is fixed Activity members, including one98-byte read-only solution mark set and one undo cell/mark. Runtime heap/flash/latency measurements on hardware remain pending; no physical resource savings are claimed.

## Weapon and place artwork

The dossier uses the supplied 20 weapon and 16 place illustrations, selected by the
actual cast entry rather than its row position. The sprite manifest explicitly
maps each crop to the English internal ID, including SPADE for the supplied
shovel and TORCH for the flashlight. The Location category uses the 16 place IDs
in MurdleCast.cpp. Motives use their own illustrations resolved by canonical ID.
Person portraits and
all deduction traits, clues and assignments remain unchanged. Existing case seeds
are preserved when the campaign expands.

Original sheets live in assets_local/murdle/source-sheets/. Reviewed per-drawing
bounds and export settings are in assets_local/murdle/sprites.json; crops are
not equal-grid divisions. The flashlight crop includes its detached light rays.
Opaque white interiors are preserved on white paper. Alpha and luminance are
thresholded to binary output without dithering, then reduced proportionally with
nearest-neighbor into 96×96 canvases with at least 4 pixels of margin. The existing
FreeInkUI Contain drawing centers them without stretching in the dossier's
portrait and landscape image boxes. Places keep their naturally wider shape.

Run python tools_local/murdle_art/extract_sprites.py, then
python tools_local/murdle_art/generate.py. The existing MurdleArt.h resource
system encodes the 48 sprites as constexpr 1-bit data and explicit ID tables.
The PNGs and large source sheets are build inputs only; firmware performs no
PNG loading, dynamic texture allocation or SD image access. The new sprite bit
payload is 55,296 bytes plus descriptors/tables, with no additional framebuffer.

The twelve motive crops use the same manifest and export pipeline. Their artwork
is selected by the canonical English motive ID, independent of the manifest's
position and translated label. Dossiers, pair tables, accusation choices and the
selected summary share that lookup. Preserve the detached highlights and symbols
and verify ANGER/HATE/FEAR/SHAME at 32 and 44 pixels. Unknown IDs return no image.

The menu illustration is the supplied study scene, exported proportionally into
a 192×240 one-bit canvas from source-sheets/menu.png. Its wider drawing column
preserves the image's aspect ratio using the existing Contain renderer.

Evidence rows retain their existing whole-row touch action and reviewed bit.
They show only the check state, a vertical separator and a vertically centered
number. The evidence heading uses the shared UI display cut, the crime-evidence
box repeats the separator, and hearts have four extra pixels of vertical room.

Verify the 20 weapons and 16 places against their labels, especially SPADE/TORCH and
OFFICE/STUDY, in both orientations. Cards must show a complete drawing with clear
white interiors, free margins and no fragments of neighbors. Existing people,
case identities, clues, lives and saved notes must remain unchanged. Actual
e-ink readability, refresh clearing and device resource checks remain pending.

## Save compatibility

Save v7 expands the completed-case bitset from 25 to 50 bytes and migrates
v2–v6 records, including current campaign positions and saved presentation seeds.
V2–v4 mistake counters migrate to at most two spent lives; v5/v6 counters above
three are rejected. Saves use one fixed State workspace and the encoded byte
buffer for read-back comparison, with no additional heap allocation. Writes
retain the existing staged verification/backup/rename protocol, dirty-write
throttle, sleep/exit flush and unreadable/future-save protection. The full
[migration mapping](murdle-campaign.md) is documented with the campaign.

## Verification and hardware handoff

host-tests/murdle checks the existing campaign solver, case fingerprints, text and codec. host-tests/murdle-redesign checks the actual screen builders with native bitmap font metrics in EN/ES and both480×800 and800×480 orientations, hit bounds/overlaps, full text, life boundaries and portrait identity. site/qa-murdle.html runs the actual browser firmware with disposable simulator fixtures for open, fresh, solved, failed and advanced cases. It clears only simulator MEMFS save paths, never a physical SD card.

On backed-up hardware: play a case, make a mark and undo; submit a wrong accusation and expect2 lives; sleep/reopen/reboot and expect those lives, names, portraits and notes preserved. Fail twice more and confirm all normal entry paths remain failed until Retry. View solution without increasing solved count. Retry the same case and expect3 lives with the same names/portraits. Solve a case and expect its check/progress after reboot. Import old v2/v3/v4/v5 saves and compare marks/progress before allowing a v7 write. Check SD removal/full card and interrupted save recovery; save errors warn that leaving can lose unsaved changes, and must offer recovery and never silently replace unreadable/future saves. Repeat in both device orientations and watch heap/largest block and watchdog logs across repeated app open/close. Browser-only verification does not establish physical SD, e-ink or resource behavior.

Verify each catalogue filter shows exactly 100 cases on seven pages, with no
phantom tiles after its last four cases. Changing filters must preserve an open
case and notes. V6 cases 51/101/151 should reopen as 101/201/301 with the same puzzle;
solved markers must follow their original puzzles after reboot.

Replay/reset and legacy save migration are specified in [the campaign guide](murdle-campaign.md).
