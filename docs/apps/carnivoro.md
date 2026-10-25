# Carnivore

Carnivore is an original offline animal-deduction game in Games. Its 1,470
cases and user-supplied pixel portraits are independent of the commercial
*A Carnivore Did It!* dossier tables, artwork and campaign scoring.

Each suspect makes one statement. Find exactly the stated number of culprits
and false statements; a culprit may tell the truth and an innocent may lie.
OR includes both; AND requires both. A false OR excludes both named suspects;
a false AND excludes at least one. Truth comparisons concern the characters'
statements, not their guilt. Trait and height statements mean at least one
culprit has that trait or exceeds/falls below the speaker's fictional height.
Names, heights and diet labels are reliable facts, separate from testimony.

The solver retains circular adjacency in fixed character order, restricted to
the current cast: Panther, Peacock, Shark, Fox, Bear, Stag, Horse. The last and
first are neighbours. Every neighbour clue displays the two actual names, with
inclusive OR, so adjacency never has to be inferred from paginated portraits.
Changing page or orientation cannot change the meaning. Each case has exactly
one complete assignment of guilt and truth, verified by the host suite.

The main menu is Open case, Campaign, How to play, Reset all. How to play has
four illustrated portrait pages followed by a Difficulties page. Landscape splits
these into eight illustrated halves and two Difficulties pages. Investigate keeps
compact statement cards with portraits, names, heights and diet labels. Portrait
shows up to three suspects per page; a case with longer statements uses two when
needed to display every word. Landscape retains one card with controls alongside.
Card heights follow the full statement in the firmware's actual prose font,
including in View solution; testimony is never shortened with an ellipsis.
Portrait cards share spare row space while keeping a gap above the centered
pager and below it before the action buttons.

View all is an optional button beside Accuse. It opens every suspect in one
registry, with separate sincerity and guilt columns. Tap a name to open its card
page, or tap a note to cycle it. Back returns to the previous card page.
Sincerity cycles Unknown / Truth / Lie; guilt cycles Unknown / Suspect / Innocent.
These are hypotheses, not automatic checks. Suspect and Innocent are mutually
exclusive. The compact cards retain direct Unknown / Truth / Lie buttons and
a guilt checkbox that cycles through the same three guilt states. Closed cases
also offer View all; their registry shows the solution and does not accept edits.
Accuse opens a separate selection screen and requires exactly the case's culprit count before
confirmation. Selecting a marked innocent for accusation clears that innocence
note. Portrait accusation shows the complete cast; landscape uses three per
page. The first incorrect accusation may leave another life; the result opens
a separate incorrect-attempt screen with the remaining lives and a Continue
investigating action. Solved and exhausted attempts have distinct result
screens. View solution explicitly opens full statement cards with truth and
culprit badges; active attempts never expose those badges.

Open case resumes the current unfinished attempt or advances through the same
progressive order as the selector after a closed case. Campaign lets you select any of the 1,470 cases from the beginning,
in any order, regardless of completion or remaining lives in another case.
Existing current notes, lives and historical results remain intact when the
same case is resumed. Every case retains its content, internal save ID and life
limit. Visible numbers follow difficulty order: Beginner 1–220, Normal 221–540,
Advanced 541–920 and Expert 921–1470. Within each difficulty, cases are grouped
by increasing culprit count, then increasing liar count; smaller casts come first
when both counts match. Cases with the same counts retain their internal-ID order.
The liar count starts from the minimum again when progressing to two culprits.
Campaign follows Murdle's picker: global solved progress, four difficulty buttons,
case cards with Unsolved / In progress / Solved / No lives left status, and a centered
pager. Portrait shows sixteen cases per page; landscape uses a sidebar and eight
cases per page. Selecting a difficulty filters the catalogue and does not alter
the current attempt, notes, results or spent lives. The existing difficulty byte
remembers the filter; opening a case selects its own difficulty. Changing filters
preserves case identities. Visible numbers remain consecutive while saves keep
the original IDs, so existing results and current notes stay with the same puzzle.

How to play ends with four difficulty cards. Counts and ranges come from the
same immutable cases at compile time: Beginner has 220 cases, 3 characters,
1–2 liars and 1 culprit; Normal has 320 cases, 4 characters, 1–3 liars and
1–2 culprits; Advanced has 380 cases, 5–6 characters, 1–5 liars and 1–2 culprits;
Expert has 550 cases, 7 characters, 1–6 liars and 1–2 culprits. Every tier starts
with one culprit; Normal, Advanced and Expert then introduce two. Within each
culprit group, liar counts increase. Each case still states its exact counts.
Beginner excludes clues about honesty; the other tiers can cross-reference
truth and lies. Existing puzzles retain their statements and solutions.

Beginner and Normal have two lives per attempt; Advanced and Expert have one.
A confirmed wrong accusation spends one life. The write is committed and read
back before displaying the verdict. Leaving, sleep or a device restart does
not restore lives in the current attempt. Completing or exhausting a case does not gate access to any other case. Replay starts a new attempt without deleting the earlier result
or affecting access to other cases. Solving a previously exhausted case upgrades it to
solved; losing a replay cannot downgrade an earlier solved result. Reset all
requires a separate confirmation and erases only Carnivore progress.

Progress lives at `/.crosspoint/carnivoro.bin`: a 2,964-byte version-5 checksum
record with 16-bit case IDs, 1,470 results and separate current-replay state.
Version-1 220-byte, version-2/3 2,024-byte and version-4 2,424-byte saves migrate
in RAM, preserving results, spent lives, replay state where available, and
current notes. Version-1/2 saves start with no Innocent marks. Version 5 retains
byte 7 for Innocent marks and expands the result/miss arrays to 1,470 entries.
All original 1,200 entries, current notes, replay state and lives are preserved
on load; the 270 appended cases start unused. Older firmware treats version 5
as newer data and preserves it.
Saves stage, read back and rename through a `.bak` recovery copy. A valid backup
can recover a damaged live file; future or unrecoverable data is preserved and
blocks mutations. SD failures block staged accusation/replay/reset results
until Retry save succeeds. There is no factory-reset escape that overwrites
an unreadable or newer save.

The immutable cases occupy 35,280 bytes of flash (6,480 more than the 1,200-case
catalogue), plus the portrait assets. The three first suspects use the supplied
redesign portraits; the other four retain their existing art. The activity
reuses three fixed 2,956-byte states and one 2,964-byte wire buffer,
approximately 11.6 KiB together, rather than putting them on the task stack.
A compile-time bound keeps the full activity within 16 KiB. Progressive selection
uses two compile-time 1,470-entry uint16_t tables and five tier offsets
(5,890 bytes before linker alignment), with constant-time lookup and no runtime
sorting. There are no additional per-frame allocations, tasks, network calls or
framebuffers. Generation is offline through `tools_local/generate_carnivoro_cases.py`;
independent fixed seeds retain existing case prefixes and reject duplicate
semantic statement sets for new cases.

Run `host-tests/carnivoro/run.sh [shot-directory]` for progressive order, consecutive
visible numbering, next-case advancement and wraparound, all solutions, semantic
uniqueness, original-prefix identity, accusation/life behavior, replay/unlocks,
v1/v2/v3/v4 migration and v5 round trips, mutually exclusive guilt notes, production
navigation and accusation actions, checksum corruption and truncation. The UI
suite uses actual firmware fonts in English and Spanish for both dimensions
and all four rotated X4 Pro safe-area configurations,
including all full statements, all case IDs, every character and its containing
page, the complete overview, paginated accusation, notes and named-neighbour clues. Its font slots
match CarnivoroActivity: slot 3 is toybox_10 and slot 4 is ui_prose_10. Settings
and Words are checked by their own suites.

On hardware, copy an existing v1 save, reopen its current case, then make a
note and verify the v5 save survives reboot. Mark an innocent, leave/sleep and
reopen: the note must remain. Cycle both note columns in the overview, open
every character by name, and confirm its entire statement is readable in both
orientations and after View solution. Returning from View all preserves the card
page and notes. Verify a seven-suspect case shows three cards on its first portrait
page and uses three pages. Test visible case 6 -> 7, tier boundaries, six or seven suspects in one overview, and first/last-character
neighbour clues. Confirm a wrong
accusation, leave/reopen/sleep, and expect the same remaining lives. Exhaust a
case, replay it, and expect every case to stay selectable. Confirm and cancel
Reset all separately. Test unavailable/full SD and interrupted renames with a
backup of your save; failed commits must not reveal an uncommitted verdict or
reset. Verify remapped buttons and both rotations, and measure heap, largest
block, PSRAM and stack high-water on entry/exit. Physical SD durability and real
panel timing require these device checks; host renders are not device shots.

Verify Campaign in all four orientations: each difficulty contains only its own cases,
all 1,470 remain reachable, first/last page arrows clamp, and current notes/lives
survive switching filters and reopening the same case. In How to play, the last
portrait page (or last two landscape pages) must show all four rules cards.

The 200 extra Beginner cases retain internal save IDs 1000–1199, three characters,
one culprit, one or two liars and the same direct-clue restrictions. They are mixed
with the original 20 Beginner cases by liar count, giving cases 1–102 one liar
and cases 103–220 two liars. Normal one-culprit cases 221–280 have one liar,
281–361 two, and 362–450 three. The 90 new two-culprit Normal cases occupy
451–540, with 30 cases per liar count. Advanced retains its 380 cases, now
541–920. Expert starts with 180 new one-culprit cases at 921–1100, with 30
cases per liar count, then its original 370 two-culprit cases at 1101–1470.
The selector, case heading and continuation share this numbering; saved results
remain keyed by the same internal IDs.

`python tools_local/generate_carnivoro_cases.py --extend-progression` retains the
original 1,200 puzzles and appends the new Normal IDs 1200–1289 and Expert IDs
1290–1469. The default generator and `--extend-beginner` reproduce all 1,470
using independent fixed seeds for the expansions. Save expansion from version 4
adds 540 bytes to each of the three existing states and the wire buffer: 2,160
extra activity bytes, with the existing 16 KiB compile-time cap retained.
No extra allocation is added.
