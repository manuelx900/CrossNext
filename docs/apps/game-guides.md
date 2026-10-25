# Illustrated beginner guides

All copy uses the game's actual rules and shared translated text, fonts,
monochrome card art, safe area and page selector. Solving, dealing, case
generation and puzzle save formats are unchanged. No new bitmap assets,
app-owned heap buffer, background task or framebuffer is introduced. The
optional Sudoku preference uses a short-lived HAL file handle on entry or selection,
including the HAL's existing small internal allocation.

- **Go (three pages):** alternating turns, fixed stones, orthogonal groups and
  shared liberties, capture, suicide and the engine's recent-position superko
  check, passing, dead-stone review and Chinese area scoring. The counting
  diagram distinguishes nine empty territory points from sixteen living
  stones. White receives 7.5 komi, or 0.5 with handicap.
- **Wordle:** the Spanish note says that Ñ can be used and vowels have no
  accents; the existing dictionary and validation are unchanged.
- **Solitaire (three pages):** a visual 52-card primer, four suits, hollow red
  and solid black symbols, A/J/Q/K names and the A-to-K order; descending
  alternating columns, same-suit foundations, stock, recycling, exposed cards
  and the four-foundation win. Only a king or a run headed by a king fills an
  empty column. The stock draws one or three according to the selected rule.
- **Hearts (four pages):** the shared card primer with ace high; how a trick
  works, following suit or discarding, point cards, rotating passes, 100-point
  or short 50-point ending, two-of-clubs opening, first-trick point restrictions,
  broken hearts and the 26-point moon rule (the shooter gets zero, opponents 26).
- **Carnivoro (five portrait pages, ten landscape pages):** guilt and truth
  are separate, OR/AND and sincerity statements, a small worked deduction,
  reliable traits and personal notes, exact suspect selection, lives and the
  1,470-case campaign. The final difficulty cards describe character, liar and
  culprit ranges; cases introduce one culprit before two (except Beginner),
  with increasing liars within each group. A taller/shorter clue requires at least one matching
  culprit. Omnivores do not meet carnivore or herbivore clues. Statements
  about sincerity can also be false. Notes are never checked automatically.

The card primer arrangement was selected from three rendered alternatives;
production retains one two-by-two suit layout and no variant macro. Other
pages use the existing illustrated lesson structure.

## Verification

Run `host-tests/game-guides/run.sh` after normal translation generation. It uses
the actual firmware prose font (slot 4: `ui_prose_10`, 28-pixel line advance),
English and Spanish, and both 480x800 and 800x480 layouts. It checks text/hit
bounds, truncation, selected Sudoku controls, correct/wrong entries that do not
necessarily conflict locally, protected clues, frozen checking and erase/undo.
It also checks that Erase highlights all player digits, leaves givens and blank
cells on white, and displays no correctness feedback outside Hint mode.
Matching-digit corner brackets can be disabled in Sudoku Settings, independently
of Erase and Hint. Settings reuses the former difficulty screen and retains all
four difficulty choices. Matching-number highlighting is enabled by default.
Each chosen difficulty or highlighting option saves immediately through the
existing per-game HalStorage configuration pattern. Back leaves Settings.
The two-byte sudoku.cfg does not change the v1 puzzle save.
It writes one-bit PBM renders to its temporary build directory.

On hardware, open every guide in both orientations and languages, step through
all pages and return with Back. Confirm that body text is readable and no
paragraph ends in an ellipsis. In Sudoku enter a number, select Erase, tap that
entry and a starting clue: only the entry disappears. While Erase is selected,
all player digits have black backgrounds and clues remain on white. Enter
correct, wrong and repeated values: no error marks appear until selecting Hint.
Then expect a check and X; board taps must change nothing.
Select a number to clear checking and resume. Leave and reopen the board to
confirm digit mode and the existing saved puzzle. Check real panel refresh and
remapped buttons. Disable and enable matching-number highlighting in Settings;
expect corner brackets to disappear and return while Erase/Hint remain the same.
Leave/reopen Sudoku and confirm that the setting persists. Browser and host
tests do not verify physical touch timing or SD durability.
