# Side-button navigation

The side buttons follow the logical Previous and Next mappings. Most short
presses finish on release; Library moves on press and repeats while held. Where listed, a hold reaches its alternate action after 500 ms and suppresses
the short action on release. Touch remains available.

Menus that still support button selection follow their visible reading order.
The current control has a black background and white text. Touch remains the
way to change the settings and ratings listed below.

| Screen | Short press | Hold |
| --- | --- | --- |
| Library | Previous or next file | Continue moving through files; keep the current section |
| Multi-page game instructions | Previous or next instruction page | No additional selection |
| Chess, Draughts, Go, Minesweeper, Connect Four and Othello settings | Touch only | Touch only |
| Solitaire and Hearts setup/settings | Touch only | Touch only |
| Picross size and Wordle letter count | Touch only | Touch only |
| Chess problem difficulty/reset menus | Touch only | Touch only |
| Chess problem picker | Change the page | Change the page once; no extra step on release |
| Chess problem during play | Previous or next problem | No menu selection |
| Picross puzzle picker, Murdle campaign and Carnivoro cases | Change the page | Change the page |
| Chess, Draughts, Go and Othello boards | Select available game controls | No board cursor |
| Minesweeper board | Select Flag or Dig | No board cursor |
| Picross board | Select Fill, Mark or Restart | No board cursor |
| Sudoku board | Select a number, Erase or Hint | No board cursor |
| Wordle, 2048, Connect Four, Solitaire and Hearts during play | Touch controls the game | No board cursor |
| Intercept story | Change the narrative page; choose by touch | No choice selection |
| Murdle case | Switch Clues, Grid and Info | No grid cursor |
| Carnivoro case | Change the page | Change the page |
| Reading Quest | Switch Hero, Missions and Equipment | No separate action |
| Habits Today and Presets | Change the page | Change the page |
| Tasks and task lists | Change the page | Change the page |
| Pomodoro timer | Start if stopped; keep a running timer running | No stage selection |
| Other Pomodoro controls | Touch only | Touch only |
| Quotes | Change the page | Switch section |
| Flashcards deck library | Change the page | No separate action |
| Flashcard question | Read pages; Next at the end reveals the answer. Either key reveals a single-page question | No rating selection |
| Flashcard answer | Change the text page | No rating selection |
| Deck cards | Change the page; ten cards per page | Hold a card with touch to open its answer |
| Review history | Change the page | Change the month |
| Words home | Select menu controls | No separate action |
| Words collection menu | Change the page | Open View List |
| Words full list and instructions | Change the page | Change the page |
| Words review question | Reveal the answer | Reveal the answer |
| Words review answer | Change the text page; rate by touch | Change the text page |
| Calendar | Change the month | No separate action |
| Clock | Toggle analog/digital | No setting selection |
| Counter | Add one | No separate action |

Search and other shared keyboard entry screens use the normal black header
with a white title and Back arrow. The selected Library cover uses a thicker
black outline with white separation. Reading statistics show five books per
portrait page or three per landscape page, with compact minute values above the min label that shrink to fit additional digits. Word
review answers use the smaller UI text face when it covers the saved text;
other scripts retain the selected reader font.

The deck-card list uses one column in portrait and two in landscape. Its labels
use the regular small text face. Sudoku settings save each chosen option without
an Apply button. Instruction pages use their page selector and the shared Back
gesture to leave.

The Library reuses its existing decoded visible rows when only the selection
changes. A new viewport, sort, filter, grouping, pinned overlay, grid view or
failed index read rebuilds that window. No second page buffer is allocated.
The change avoids repeated index reads during cursor movement; physical refresh
time still depends on the display.

Horizontal page swipes follow the visible page controls across Words,
Flashcards, Quotes, Wallpapers, Notes, Tasks, Habits, Reading Quest, Carnivoro,
Murdle, The Intercept, Picross and Chess Puzzles: left advances and right goes
back. Card text stays on its current question or answer; ratings remain touch
controls. Library cover grids use horizontal rather than vertical page swipes.
Reading Stats uses horizontal swipes for Today dates, completed/recent book
pages, calendar months and the selected week/month/day/year detail period.
Global edge gestures keep Back, Home and the quick panel ahead of page swipes.

Opening Wi-Fi management shows the radio's current state. An explicitly disabled
radio stays disabled on returning to that screen, including while Developer Mode
is enabled. Enabling Wi-Fi or starting a connection scan allows Developer Mode
to use the radio again. This manual-off latch lasts for the running session.

Device verification should cover both short presses and holds, release after a
hold, touch followed by keys, remapped buttons, all four orientations, and empty
or multi-page lists. After disabling Wi-Fi, leave and reopen its management
screen: it should still say Off and show no active network scan.
