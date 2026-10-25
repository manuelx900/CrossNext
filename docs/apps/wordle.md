# Wordle

> Current profile (October2026): Wordle selects English or Spanish dictionaries from the interface language, with separate 4/5/6-letter answers and accepted guesses. Ñ is distinct; enter vowels without accents. Six valid guesses: black means correct place, gray means present elsewhere, white absent. Unknown words remain editable without spending a try. Replacing unfinished games after a length or vocabulary change requires confirmation. The sections below retain background on the original English implementation.

Wordle is offline free play with six guesses and a choice of four, five or six letters. Play resumes an unfinished word of the selected length, or starts a new one. Word Length shows four-, five- and six-square cards. Apply stores the next game's length; Back discards the draft selection. Starting a different length while another word is unfinished asks for confirmation and counts the abandoned game as a loss; Cancel preserves it.

The local vocabulary contains 2,465 common English words: 615 four-letter, 973 five-letter and 877 six-letter words. It is an original curated selection, distributed under the project's MIT license, not a downloaded NYT answer archive. The same bank validates guesses. Invalid words remain editable. An answer imported from an old save is also accepted even if it is outside the new bank. Each length uses a saved permutation of its bank, avoiding repeated answers until that bank has been exhausted, then cycling. This is a local game, not a promise to match a daily puzzle elsewhere.

Correct-position letters are white on black, letters in another position are dithered, and absent letters remain white with an outline. The QWERTY keyboard uses the same marks. OK submits; the erase key removes a letter. The board and keyboard adapt to portrait/landscape. At the end the grid retains the attempt count and the result area reveals the answer; Menu and Play Again replace the keyboard.

The menu shows one row of Played, Win %, Streak, Max Streak and the distribution of successful attempts. Played counts finished games and confirmed abandonments. Streaks count consecutive wins across completed local games, regardless of calendar date. Exiting and resuming a word does not count a loss. Each completed game contributes once. Counters are capped at one million completed games to keep record arithmetic and display bounded.

## Ownership and persistence

Core state is 196 bytes and the two-slot repository workspace is 468 bytes on the host ABI, excluding UI/activity fields. The 2,465-word bank lives in constexpr flash data. No radio, worker, network request or growing game-history file is used. The reused Game text codec constructs a bounded temporary string (at most 48 characters) during cold save/verification paths, not while rendering. Typed input is debounced for two seconds and flushed on submitted guesses, length selection, exit and sleep. No per-frame container growth is added.

Two verified 256-byte WDL1 records live at `/.crosspoint/wordle-local-a.bin` and `wordle-local-b.bin`. They contain generation/checksum, chosen length, random-bank seed and positions, aggregate statistics, current answer/guesses and partial typing. A torn write falls back to the older valid slot. Unknown future records and wholly damaged records are not overwritten and show Save Unavailable. A valid board is reconstructed through the normal rules so tiles and keyboard marks cannot disagree with saved guesses.

When neither local record exists, legacy `wordle.res` (up to 64 KiB) imports its completed-on-the-day results once and `wordle.games` (up to 256 KiB) supplies the newest unfinished five-letter attempt. The original legacy files are preserved. Older, larger files are left untouched. After a verified local save, subsequent entries use that save, preventing duplicate import. Statistics now use consecutive completed games rather than the legacy daily calendar rule.

Vocabulary sources are `assets_local/wordle/words-4.txt`, `words-5.txt`, and `words-6.txt`. Run `python tools_local/wordle/build_words.py` to regenerate the header; `--check` verifies it. The generator enforces sorted unique uppercase ASCII words of the expected length and a maximum of 3,000 entries. Changing bank ordering/counts requires deliberate save-format/data-version handling because saved permutation positions depend on it.

## Verification

Host checks cover every word, a complete nonrepeating cycle for each length, scoring/wins, partial-input persistence, record-once statistics, abandonment, checksums, torn writes, future records and bounded legacy import. UI checks cover keyboard hit mapping and completed-game controls. On device, play each length, repeat letters, enter invalid/short words, resume after restart/sleep, confirm and cancel a length switch, and check win percentage/streak resets. Test missing SD and both orientations. Observe heap/largest block, stack and physical e-ink updates. Hardware verification is pending.

Native builds and the browser build generate the word bank from its text sources automatically. The local Wordle host suite does the same; --check is a deterministic generated-output check.
