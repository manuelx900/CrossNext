# Picross

The main menu has Play, Select puzzle, Size and How to play. The example
diamond teaches clue reading without revealing the selected puzzle. Play opens the
selected puzzle and resumes its board if it is already in progress.

On the grid, a short touchscreen tap uses the selected Fill or Mark tool.
Hold a cell for half a second to toggle an X mark; lifting the finger after
the hold does not fill it. The Fill/Mark controls and side keys select the
tool used by taps and physical-button activation.

Select puzzle shows numbered pages within the chosen size, with completion and
in-progress marks. Tap a number to open that puzzle. Every one of the 450 puzzles
is selectable immediately. Size shows grid cards and cell totals. Selecting a card applies and saves the size immediately. Only its checkbox is filled, with a white checkmark. Back returns to the menu; changing size preserves the saved puzzle. The one-byte preference is stored separately at `/.crosspoint/picross-size.cfg`; a missing or invalid preference uses the saved puzzle or next unsolved size.
Changing to a different puzzle replaces the single in-progress board after saving
the current one; completed puzzles stay recorded. Returning to the same puzzle
keeps its current board. How to play has four illustrated pages covering clue runs, gaps, Fill/Mark/Selection and the completed diamond. It uses the shared bottom page selector.

The upper-right battery follows the device visibility setting. The Sudoku header
uses this same battery position; its difficulty remains in the game menu.

On X4 Pro, test both orientations, all sizes and the first/last selector pages,
tap another number to open it, return with Back and resume the saved board. Check
that physical side keys page in the selector and switch the Fill/Mark tool
while playing. Check a held cell marks once, its release does not fill, and
a fresh short tap marks when Mark is selected or fills when Fill is selected.
