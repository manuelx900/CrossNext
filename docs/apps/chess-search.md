# Chess search and local play

Person versus person keeps White at the bottom throughout the game. Both sides can move on their own turn. Computer games keep the human side at the bottom.

Move history remains canonical English SAN on SD. Spanish UI translates the displayed piece initials (C, A, T, D, R), including promotions. Switching language never rewrites a save.

Easy retains its original evaluation, move ordering, one-ply target and 2,500-node ceiling. Normal (Medium) and Hard use the stronger positional evaluation and search below. No rules, settings or saved-game formats change.

| Level | Regular depth target | Total node ceiling | Cooperative time budget |
| --- | --- | --- | --- |
| Easy | 1 ply | 2,500 | Existing node limit |
| Normal / Medium | 5 plies | 60,000 | 1.2 seconds |
| Hard | 6 plies | 180,000 | 2.5 seconds |

A ply is one player's move. The targets are ceilings, not guaranteed completed depths. All levels follow captures, promotions and check evasions for up to four additional plies. Mate and stalemate are checked even at that limit. An interrupted iteration is discarded; the last complete result is played, or a legal fallback if no iteration completed.

Medium and Hard blend middle-game and endgame values. Kings become active in endings, passed pawns matter more as they advance, isolated/doubled pawns are penalized, and bishop pairs, mobility, rook files and pawn shields influence the position. A rook or queen against a bare king rewards bringing the king closer and driving the defender toward an edge.

Principal-variation search uses narrow windows for alternatives and re-searches moves that improve the current choice. A fixed 512-position cache remembers promising moves, and two quiet cutoff moves per ply help ordering. The cache deliberately stores no scores: draw values depend on the search path and the caller's repetition history. Medium and Hard recognize threefold repetition and the 50-move rule within the available history; checkmate takes priority. The existing limitation after resuming a save remains: repetition history before reload is not reconstructed.

The Activity searches a separate board copy. A synchronous callback checks the time budget every 256 nodes and yields to FreeRTOS about every 20 ms of elapsed work. This keeps the existing render task available without adding a worker. The deadline is cooperative and can overshoot by a node batch or scheduler delay; actual device latency and stack headroom still need measurement. Debug logs include elapsed milliseconds, visited nodes and completed depth.

The normal game owns an `OpponentBuffers` workspace for its lifetime, including on Easy. Its measured host size is 19,680 bytes, replacing the 9,252-byte legacy workspace: an increase of 10,428 bytes in the existing fallible Activity allocation. Fixed member storage avoids per-move allocation and recursive MoveLists. Problems keeps the original `SearchBuffers`, reply search and memory size, including while pushed over a retained Chess game. No new app-owned allocation, task, framebuffer, dependency or SD resource is introduced. Native sizes, flash headroom and runtime heaps are recorded separately during verification; host sizes are not hardware heap measurements.

Run `host-tests/chess/run.sh` and `host-tests/chess-puzzles/run.sh`. The opponent checks include tactical sequences from the existing CC0 catalog, active kings, an independent full-width reference search, repetition windows, protected workspace boundaries, cancellation and input-position restoration. A queen/rook pair checks both taking a free rook and capturing its knight defender instead when taking the rook would lose the queen. A pre-change comparison uses a fixed balanced 120-problem sample and paired games with reversed colours; it is a node-limited host diagnostic, not an Elo rating or a device timing claim.

On hardware, play Medium and Hard with each colour and in both orientations. Confirm that Thinking precedes the reply, the board stays stable while searching, and taps during Thinking do not select a square in the next turn. Check the elapsed-time/depth logs in complex middle games and king/pawn endings. Monitor internal/PSRAM free heap, largest blocks and stack high-water through repeated Chess entry/exit and entry into Problems over a live game. Save/reopen and both human-player mode and Problems should behave as before.

Checkers uses the same concentric disk renderer on the board and in remaining-piece counters; a small additional centre ring distinguishes kings. Its rules and orientation are unchanged.
