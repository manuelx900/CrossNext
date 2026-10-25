# Connect Four

The menu offers Play, How to play and Settings. Settings chooses the computer or two people on this device, Easy/Normal/Hard and your colour for the next game. Its illustrated heading sits above joined horizontal choices for Opponent, Level and Your colour, with Done fixed at the bottom. Nearby radio play is removed. Local two-person results do not change the computer-game win history.

The rules, exactly, and the two state machines the app is built on. Read this
before touching `src/apps_local/connectfour/`.

---

## 1. The rules

Seven columns, six rows, forty-two places. Drop a disc into a column and it
falls to the lowest free place in it. Four of yours in a line, in any of the
four directions, wins. A full board with no line is a draw.

That is the whole game. There is no [house] section, because there is nothing
the standard rules leave for a handheld to decide: no draw-by-repetition
question (the board only ever fills), no time control, no arbitration.

### The board is stored bottom-up

`cell[column][row]`, **row 0 at the bottom**, because that is the direction
gravity works in. `landingRow` is then a scan from zero, and the screen does the
flip in one line (`drawRow = kRows - 1 - row`).

`testRowZeroIsDrawnAtTheBottom` exists because if that flip ever inverts,
discs pile downward from the ceiling and nothing else in the app notices: every
rule, every test and every packet still agrees with itself.

### Nothing can float

The one property built in rather than checked for. A caller names a **column**,
never a row; the row is computed by `landingRow` and there is no other way in.
A floating disc is not a bug that is prevented, it is a state that cannot be
expressed.

`plausible()` re-checks it anyway on anything arriving over the wire, because a
corrupt packet does not go through `drop()`.

### Light opens

And that matters more than it looks. Connect Four is **solved**: the first
player wins with perfect play. Randomising who opens would not make the game
fair, it would only hide the asymmetry from whoever lost. So light always opens
and the player chooses light or dark in Settings. When the player takes dark, the computer opens. In person-versus-person mode either player taps their moves on the same fixed board.

---

## 2. The game state machine

`connectfour::Phase`, in `ConnectFourFlow.h`. Three states, none of them stored:

```
Yours ---- you drop ----> Theirs
Theirs --- they drop ---> Yours
either --- four, or a full board ---> Settled
```

`phaseFor(over, yourTurn)` derives it. There is no `phase_` member, so there is
nothing that can disagree with the board.

**A win keeps the turn.** `drop()` settles the outcome _before_ handing the turn
over, so `game.turn` still names the winner when the game ends. Everything reads
"whose" from that field, and a winner that passed the turn on would announce the
wrong name on the result screen.

---

## 3. Local activity

The activity owns its input table and invokes the computer after a legal move.
There is no discovery, radio session or rematch negotiation. The result screen
records the finished board once, then offers a new game or a return to the menu.

## 4. The opponent

`ConnectFourBrain.h`. Deterministic, pure negamax with alpha-beta. Easy, Normal
and Hard search three, five and seven plies respectively; Normal is the default.
The maximum remains seven plies, with no new search allocation.

Connect Four has no hidden information, so "it cannot cheat by looking at what
you hold" is free here in a way it was not in Jaipur or Knucklebones. What is
not free is the other half of that promise.

- **It only ever plays a legal column.** Its choice comes from the same
  `canDrop` the board uses.
- **It is deterministic.** Same position, same column, and it does not touch the
  board while thinking. Every screenshot recipe in this repo replays a game by
  repeating taps, so a brain that rolled a die would make the site
  unreproducible.
- **It takes a win and blocks a loss**, tested from both colours so neither is
  an artefact of the other.
- **It does not hand over a win** it could have refused, which is the first
  thing a purely material evaluation gets wrong.
- **It beats a random mover 40-0 of 40**, with the sides alternating so the
  first-player advantage is not doing the work.

The last one is the test that matters, and the reason it is stated as
`randomWins == 0` rather than "more than half": Checkers had a brain whose
evaluation had been negated, and _legal_ and _deterministic_ both stayed green.

Three details worth keeping:

- **Deeper wins score lower** (`kWinScore + depth`), so a mate in one beats a
  mate in five and a loss is postponed as long as possible. Without it, every
  winning line scores the same and the brain finds a win and then wanders.
- **`kLinesThrough`** is the number of possible fours passing through each
  column: `{3, 4, 5, 7, 5, 4, 3}`. The centre preference is a fact about the
  board rather than a hand-tuned bonus, and it is also what breaks ties.
- **`kSearchOrder` is middle-outward** purely so alpha-beta prunes more. It does
  not change which column is chosen, because ties are broken explicitly.

---

## 5. The menu state machine

```
Menu ----> Board ----> Result ----> Review
  |          |            |           |
  +-> HowTo  +--- Back ----+---> Menu  +-- Back --> Result
```

`back()` is an exhaustive switch with no `default`. `leavesApp()` is true only
for `Menu`: the first Back means stop playing, the second means leave.

## Screen layout

The menu shows games, wins and the current winning streak, followed by the last
finished position. A new board uses dots for empty cells, outlined discs for
white and solid discs for black. Numbered column buttons and the grid
share the same drop targets. The turn band shows whose move it is and the move
number. Full columns remain visible with a crossed-out button.

Wins, losses and draws have separate result layouts. View board opens the
finished position with the winning line; Back returns to the result. Another
game starts a fresh board. The three help pages explain dropping, all three
line directions, full columns and draws. The trophy and crossed flags come
from the supplied redesign illustrations.

History appends the winning streak after the original fields. Older history
files still load, with a zero streak until new games are recorded. No active
game is persisted.

The four-byte version 2 `/.crosspoint/connectfour.cfg` stores version, opponent mode, next-game colour and level. Version 1 three-byte preferences retain their rival/colour and use Normal. Unchanged preferences do not write; active matches retain the choices with which they started.
