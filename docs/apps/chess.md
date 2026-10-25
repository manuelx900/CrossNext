# Chess navigation

The start menu offers Continue when a saved game exists, New game, Problems and Settings. There is no Take back option. Back from a running board saves and returns to the Chess menu; Back from that menu returns to Games.

Settings groups Computer or Person, Easy/Normal/Hard, Your colour and two independent helps: Show possible moves and Highlight available squares. The level preference remains visible in Person mode for the next computer game. Each help has its own framed row and checkbox. Only Back leaves this screen; taps in empty space do nothing. The footer says Back to menu. Changing opponent or the computer player's colour starts a new position when Settings closes. In Person mode, changing Your colour sets the win/loss perspective without discarding the board. Old face-to-face preferences migrate to Person mode.

Classic is the only piece design for normal Chess, menu previews and Problems. Old Pixel/Ornate preference values fall back to Classic; no design selector remains. The sixth compatibility field of `/.crosspoint/chess.cfg` stores Classic, and the seventh stores the independent highlight preference. Older four-, five- and six-field settings remain readable; a missing highlight field follows the old hints preference.

The larger board has coordinates outside its edges. The panel below shows the latest SAN move and lost-piece icons with counts for both colours, including zero. Explicit capture counters distinguish captures from promotions. Each piece uses an opaque silhouette pass and an ink pass, keeping it legible on the gray dithered squares. The immutable 64px masks and result illustrations live in flash; rendering adds no heap buffer, task or framebuffer.

Completed games open You won / You lost with a separate winning-colour sentence and the supplied coloured victory/defeat artwork. Draws state their reason. New game starts a fresh board; View board keeps the final position read-only; Back to menu retains Continue. Recreating Chess preserves the completed result, including a threefold-repetition draw.

Game saves at `/.crosspoint/chess.sav` use version 3: FEN, history base, ten capture counters, terminal draw reason and SAN history. Version 1 saves remain readable with counts estimated from missing material, which cannot recover all historical promoted-piece captures. The interim version 2 capture format remains readable. Live repetition history before a reload is not reconstructed; an already-completed repetition result is retained by version 3.

[Problems](../chess-puzzles.md) opens difficulty selection before its numbered picker. Completed cells are black with a white check and remain replayable. Normal game saves are independent of problem progress. `tools_local/chess_reference/generate.py` regenerates the Classic masks; `tools_local/result_art/generate.py` regenerates the additional supplied result illustrations.

Run the dedicated redesign, Chess and puzzle host suites for rules, captures, classic migration and real-font screen checks. Browser checks cover navigation, both colours, last move, capture persistence and results. Device flash headroom, heap, stack and e-ink timing remain unmeasured. On hardware, check all orientations, both player colours, the two helps, exit/reopen after capture and after repetition, a fresh game's completed result and entry/exit of Problems.
