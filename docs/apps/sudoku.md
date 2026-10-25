# Sudoku

Classic 9x9, generated on the device and graded by the hardest technique it
actually requires. Solitaire: no link layer, and that is a decision rather than
an omission.

Four layers, as usual: `SudokuCore` (the rules, freestanding), `SudokuGame`
(what the player touches, freestanding, and also the save), `SudokuScreens`
(freestanding builders), `SudokuActivity` (the only part that knows about
hardware).

## The promise the whole thing exists to serve

**A puzzle is exactly as hard as its label says.** Not estimated from the clue
count, which is the usual dodge and is close to meaningless: the generated
puzzles average 27.5 clues at every level, EASY and EXPERT alike.

Difficulty is the hardest technique a puzzle _requires_, and both halves of that
are established rather than hoped for:

- **The ceiling is constructed.** The dig is driven by a bounded solver, so a
  puzzle can never need a technique above its band. If the solver cannot finish
  the grid without the clue that was just removed, the clue goes back.
- **The floor is proved.** `carve()` grades the finished puzzle with the same
  solver and reports what it made. `generate()` discards anything that does not
  grade as asked. Nothing unverified reaches a player, and nothing is ever
  relabelled.

Uniqueness needs no separate check at all, which is the neatest part: a puzzle a
technique solver drove to completion was _forced_ at every step, so it has
exactly one solution by definition. `host-tests/sudoku` still verifies it with a
brute-force solution counter that shares no code with the solver making the
claim, because a property checked only by the thing that promises it is not
checked.

## The ladder

Eleven techniques, ordered by how hard a human finds them. That order **is** the
difficulty scale: `solve()` always applies the cheapest technique that fires and
restarts, so the hardest one it ever needed is a measurement.

| Band   | Techniques                                              |
| ------ | ------------------------------------------------------- |
| EASY   | naked single, hidden single                             |
| MEDIUM | locked candidates (pointing and claiming)               |
| HARD   | naked pair, hidden pair                                 |
| EXPERT | naked triple, hidden triple, X-wing, XY-wing, swordfish |

**Expert is deliberately the widest band, and the width is a measurement.** Each
of triples, X-wing, XY-wing and swordfish is individually uncommon. With Expert
holding only naked triple and X-wing, the generator carved one puzzle in forty
attempts at 939ms each. Widening the band took that to 12.8ms. The band got
wider; what EXPERT promises did not get weaker.

## Two things that were tried and are wrong

**A two-pass floor construction does not work.** The first design dug to
maximality at the band _below_ the target, then continued with the target's own
ceiling, on the argument that any clue removed in the second pass provably takes
the puzzle above the lower band. The argument is sound and the algorithm is
useless: the first pass digs to maximality and the second pass finds nothing.
Measured over 200 grids per level, clue counts went 27.8 in and 27.8 out, and
only 3% of attempts removed anything at all. One maximal dig at the full ceiling
plus a grade is simpler and three times cheaper.

**The greedy dig really is maximal.** Re-sweeping the same clue list in another
order finds nothing, because removing clues only ever makes a puzzle harder, so
a pair rejected once stays rejected. That is worth knowing before anyone tries
to squeeze the clue count down; getting below about 27 symmetric needs search,
not another sweep.

## The measured distribution

A symmetric maximal dig at the full ceiling, over 1500 grids:

| hardest technique required | share |
| -------------------------- | ----- |
| singles                    | 83%   |
| locked candidates          | 9.6%  |
| pairs                      | 3.0%  |
| triples, fish and XY-wing  | 4.4%  |

So EASY costs about one attempt and HARD about thirty, at well under a
millisecond each. `host-tests/sudoku` prints the per-level cost on every run, so
a change that wrecks it is visible rather than silent. On the host: 0.8ms EASY,
8.6ms MEDIUM, 15.6ms HARD, 12.8ms EXPERT.

Generation runs one loop pass _after_ the frame that says `MAKING ONE`, with a
budget of 24 attempts per pass and the activity looping across passes, so the
main loop is never pinned for the whole carve.

## Screen layout and controls

The menu shows the solved count, best unassisted time for the chosen difficulty,
and the current puzzle or an example. Settings contains the next puzzle's
difficulty and an optional matching-number highlight. Apply commits the choices;
Back discards them. Existing
levels, including Expert, remain available. The saved puzzle is preserved
until a new puzzle is actually generated.

In portrait, the board uses one row of nine number keys, then Erase and
Hint. In landscape the number pad uses three columns beside the board. Select
a number and tap a cell to write it; tapping a clue selects its number. Tap an
entry containing the selected number again to clear it. Erase is a persistent selected tool: tap editable cells to remove entries or
notes. While Erase is selected, player entries have a black background and white
digits; starting clues and empty cells stay on white. Starting clues remain
protected. Hint is a selected checking tool:
correct player entries show a check and wrong entries show an X against the
unique solution. Board taps do nothing while checking. Select any number to
leave either tool and clear the checking marks. Using Hint with player entries
counts as assistance; it never fills in an answer. These tools are transient
activity state, reset on leaving the board, and add no fields to the save.
Ordinary digit mode highlights matching numbers without flagging errors. The
check and X appear only in Hint mode, including for repeated numbers.
The matching-digit corner brackets can be disabled with Settings > Highlight
matches. This defaults to enabled and does not affect Erase or Hint feedback.
The preference is independent of the puzzle save: `/.crosspoint/sudoku.cfg`
contains two bytes, version 1 and the enabled flag (0 or 1). Missing, truncated
or invalid data uses the enabled default. Only an applied change writes this
file, through HalStorage. A failed preference write leaves Settings open with
an error and keeps the active choices unchanged so Apply can be retried.

The timer advances only while solving. It is painted with game interactions,
without a periodic e-ink refresh. Completing a puzzle opens a separate result
with difficulty, elapsed time and the total solved count. View board returns
to a protected finished grid; View result reopens the result without recording
the solve again. Four illustrated help pages cover rules, number selection,
erasing and explicit checking with Hint. No device refresh or touch timing has been
verified by the browser preview.

## The save

One line of small integers, then the clues, then the player's digits, then their
marks as three hex digits each. About 450 bytes.

**The answer is not saved, it is re-derived.** The puzzle has exactly one
solution, so the clues determine it; a save that cannot be solved is a corrupt
save rather than a puzzle with a surprising answer. That is also what makes the
file independent of the generator: a later generator changes nothing about how
an old grid reads. The level and the hardest technique are re-graded on load for
the same reason -- there is nothing in the file that can disagree with the
puzzle.

Except one field, which is why the load ends by overruling it. `menuLevel` is
what the NEXT puzzle will be, and it is stored rather than derived, so a card
written while the player was browsing the difficulty row holds a level the saved
grid does not have. The load snaps it back to `puzzle.level` whenever there is
an unsolved game, so the headline, the caption and the door all describe the
same puzzle. A level picked and never played is not worth carrying across a
restart; a half-solved puzzle is.

## RESUME or NEW PUZZLE is derived, never latched

The front door is one button whose label is a fact about the save:
`sudoku::canResume()` asks whether there is an unsolved game whose
`puzzle.level` is the level the menu is showing. `SudokuActivity` and
`buildMenu` call the same predicate, so the label a player reads and the door
they get cannot come apart.

It used to be a flag set on every DIFFICULTY tap and cleared only by entering
the app or starting a game. The row steps `(level + 1) % 4`, so **four taps
returned the menu to the puzzle's own level with the flag still set**: the grid
was still drawn, cell for cell, above a button that now overwrote it, with no
confirmation. A latch cannot represent coming back to where you were, which is
the whole of the bug. `host-tests/sudoku` walks a full lap of the row from every
starting level.

## Why there is no PLAY NEARBY

Asked and answered rather than skipped. Sudoku is a solitaire, and the only
shape a second device could take is a race, which is _continuously_
simultaneous where `linkplay` is strictly turn-based. Battleship's trick of
splitting a simultaneous phase into two moves works because placing a fleet
ends; racing does not. Forcing it through would be new work in the link layer
rather than in the game.

## Reproducing the screenshots

`tools_local/sudoku/sudoku_mkstate.cpp` writes a save from a fixed seed, so the
same argument gives the same grid every time:

```bash
c++ -std=c++17 -O2 -Isrc/apps_local/sudoku \
  src/apps_local/sudoku/SudokuCore.cpp tools_local/sudoku/sudoku_mkstate.cpp -o /tmp/sudoku_mkstate
/tmp/sudoku_mkstate midway > fs_agent/.crosspoint/sudoku.sav
rm -f fs_agent/.crosspoint/shelf.cfg
```

That second line is not optional and it cost a render to learn: **the shelf
remembers which folder page and row you were on, and it survives across runs**,
so a tap script that pages is only reproducible from a cleared card. Without it
the second run opened Jaipur and the screenshot looked perfectly plausible.

## Tests

- `host-tests/sudoku/run.sh` -- the rules and the game. Uniqueness by an
  independent brute-force count, the ladder property at every level (solvable at
  its own ceiling, **not** solvable one band below), symmetry, hints played out
  until the grid fills, no dead taps on any cell in any state, undo restoring
  every digit and every mark exactly.
- `host-tests/ui/run.sh` -- the screens. Both hit tests as exact inverses over
  every pixel, the four discrete controls, result access on a solved grid,
  and the preview changing when the game does.
- `host-tests/connect-sudoku-redesign/run.sh` -- both aspects and languages,
  number-key bounds, Erase controls and absence of Notes, result routes and illustrated helps.




New Game is always available on the menu. Replacing an unfinished puzzle asks for confirmation; Continue then opens the newly generated puzzle. Cancel retains the current puzzle.

The Balanced menu keeps its preview board fixed. Its action block is aligned at the bottom with a gap below the preview; the old short rules caption is removed.

The first How to play page sizes the column diagram numerals to its small cells; the row and 3×3 examples retain their space.
