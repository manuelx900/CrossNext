# Battleship

The local game opens with statistics and a coordinate-labelled fleet preview.
New game opens placement: select a ship on the board or in the five-ship strip,
tap its destination, and tap the ship again or Rotate to turn it. Shuffle and
Ready remain separate actions. Ships cannot overlap or leave the board.

During play, one large coordinate-labelled grid uses the available space.
**Your move** holds the current instruction, selected target or shot report.
Select an unfired rival cell, then Fire or tap that cell again. Fleet counters
show ships still afloat, out of five. Tap **You** to inspect your full-size
read-only fleet. The explicit **My board** button opens the same view; it changes
to **Rival board** to return to firing. The Rival counter also returns there.
Opponent ship positions stay hidden until shot or the game finishes. The
in-game Water/Hit legend and miniature ship strip are omitted; the illustrated
help retains the symbol explanation.

A finished game opens a victory or defeat screen with New game, View board
and Back to menu. Reviewing the final boards does not record the result again.

BattleshipScreens draws the selected grid and returns its rectangle. The activity
uses those same rectangles to map taps, while retaining ownership of game
state, computer turns and SD persistence. Rules, turn order and save formats
are unchanged. The reference art is a constexpr 1-bit mask in flash and adds
no render-loop heap allocation, task or framebuffer.

Host checks are in `host-tests/battleship` and
`host-tests/mystery-naval-redesign`. Test both orientations, selecting and
rotating every ship, an invalid placement, an aimed shot, a miss, hit and sunk
ship, and resume after leaving. The UI suite also compares two hidden enemy
fleets before any shot: their target-grid pixels must be identical. Physical
refresh timing, button mapping and SD behavior still require device testing.
