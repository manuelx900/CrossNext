# Battleship

The local game opens with statistics and a coordinate-labelled fleet preview.
New game opens placement: select a ship on the board or in the five-ship strip,
tap its destination, and tap the ship again or Rotate to turn it. Shuffle and
Ready remain separate actions. Ships cannot overlap or leave the board.

During play, the opponent grid is the large touch surface. Selecting an
unfired cell displays its coordinate and a separate Fire action. The smaller
own-fleet chart is read-only. Opponent ship positions stay hidden until a cell
is shot or the game finishes; the fleet strip reports sunk ships. A shot
report remains separate from the fire action, with the supplied sinking
illustration for a sunk own ship. The single illustrated help page explains
placement, readiness and firing, with the Water/Hit legend.

BattleshipScreens draws both grids and returns their rectangles. The activity
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
