# Murdle campaign

The catalogue contains 400 immutable seeds: 100 each for Elementary, Nosy,
Hard Boiled and Impossible. All cases are available from the start. Each tier
has seven 4×4 pages: six pages of 16 cases and a final page of four. Completion
uses a 50-byte bitset. The first 50 seeds of each tier are unchanged.

`tools_local/generate_murdle_campaign.cpp` deterministically generates the 50
additional seeds per tier on a host. Host tests regenerate all 400 cases twice,
check unique solutions, every clue and minimality, and solve each by deduction.
The generator and logical fingerprints are unchanged.

Tap a completed tile marked Replay, or Replay case on the solved result screen,
to play it again with fresh notes and three lives. Completion stays checked and
does not increase the catalogue's completed count. Result history and attempt
statistics still record each play. Replaying the loaded case retains its names
and portraits; campaign cases generated anew use a deterministic presentation
seed. Replacing a different unfinished investigation asks for confirmation.

Reset in the catalogue opens a confirmation showing the completed count. Keep
progress or the header Back cancels. Reset everything clears completion, the
active investigation, attempts and result history, then saves immediately. All
400 cases remain available. Storage errors use the existing retry/error UI.
Only the top pager remains on the Clues screen.

Save v7 uses a 16-bit campaign index and reads v2–v6. Old blocks of 25 (v3–v5)
or 50 (v6) retain their seeds at the start of each new 100-case tier. Completion,
current case, notes, names, portraits and lives migrate together; v6 case 51
becomes 101 and v6 case 200 becomes 350. V2 random cases remain independent.
V2–v4 mistake counters migrate to at most two spent lives. The no-case sentinel
is 65535; the old byte sentinel 255 never becomes a valid case during migration.

Writes stage and verify a replacement before renaming with a backup. The loader
recovers a valid backup after an interrupted rename. It preserves unreadable and
future records. Progress saves after verdicts, replay/reset, every three seconds
when dirty, on exit and before sleep. A failed write retains dirty state and
shows the storage error rather than claiming the reset is durable.

English and Spanish templates share the same logical clues. Host tests sweep
phrase forms and render both languages and both orientations without truncated
controls. Physical X4 Pro checks remain necessary: migrate a backed-up v6 save,
replay a solved case, reboot, cancel then confirm reset, reboot again and verify
400 available cases / zero completed. Check SD removal/full-card recovery and
lives/notes before and after sleep. See [UI details](murdle-ui.md).
