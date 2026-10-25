# Othello

Othello is a Games shelf item using PocketGamesActivity, one framebuffer and bounded activity-owned workspaces. There is no game task or network request. Board Back opens the menu; menu Back returns to Games.

## Othello

Settings offers Easy, Normal and Hard, plus black or white for the next new game. Black starts; when the player chooses white, the computer makes the opening move. Restart preserves the current game's colour. The paused menu shows both counts and the saved board with coordinates. Continue resumes it; New Game and Restart show the current board before asking to replace it. Cancel keeps the current game.

On your turn, dots mark legal moves. Tap one to enclose opposing disks without gaps; all enclosed lines flip, including diagonals. Your completed move stays visible for at least one second after its display refresh finishes before the computer replies. The computer then places its new disk in a framed square with a contrasting centre mark. Existing disks and counts stay unchanged for one second after that placement is drawn; only then do the enclosed disks flip and the counts update. Menu, Back and Restart remain usable throughout. Leaving or sleeping during a placement cancels that preview and retains the last committed board; the computer chooses again on return. Board touches cannot queue a move for your next turn. A player with no legal move passes automatically; consecutive computer moves use the same placement sequence. The game ends when neither player can move, even with empty squares. More disks wins; equal counts draw.

Completion shows the final board for the same interval before opening a separate win, loss or draw screen with the final counts. View Board shows the protected final position; Back returns to the result. The menu can reopen a finished result. Three illustrated help pages use the shared previous/current-total/next pager; logical page keys work there too. Portrait follows the supplied monochrome designs; landscape puts the board beside its controls. The trophy, flag, pass, end and direction illustrations are one-bit reference assets, without a second framebuffer or a game task.

## Persistence and verification

Othello retains its two 256-byte PGM1 slots at `/.crosspoint/othello-{a,b}.bin`. Its kind value (1), checksum and slot size remain compatible. Othello uses previously zero payload byte 67 for the human colour (1=black, 2=white); older zero values mean black. Its three-byte `othello.cfg` stores version, default difficulty and next-game colour. Unchanged preferences do not write. No-op saves do not write. Menu, sleep, exit and completion flush; dirty Othello progress is saved after three seconds. A torn latest slot recovers the other valid slot. Damaged/future data stays read-only with a warning. Saves from retired games are left on the SD card and are never opened by these games.

Host tests cover complete Othello games, corruption, redundant writes and torn saves. The Othello redesign suite checks its screens, help pages, touch targets and board hit mapping in both orientations and languages. Physical-device verification remains outstanding.

Device verification remains necessary: test touch and logical board controls, sleep, new-game cancellation, re-entry/reboot, and Othello flips/passes/end states. Use disposable saves for interruption tests; monitor internal heap, largest block and task stack over repeated entry, plus latency, ghosting and active-play power. Browser checks do not establish hardware behaviour.

Easy prefers flips; Normal values corners and edges; Hard also evaluates the strongest immediate reply. This is a bounded local heuristic. Host checks play complete games with both human colours and all three difficulties.

Othello Settings uses an illustrated heading, joined Easy/Normal/Hard buttons and White/Black disc choices. The next-game colour note appears above the bottom Done button. The existing difficulty and colour preferences remain the source of truth.
