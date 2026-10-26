# XTC and XTCH reading

Run `./host-tests/xtc-reading/run.sh` from the repository root. The test compiles the production XTC parser, reading-session accumulator and statistics repository against an in-memory storage boundary. It generates both bitmap formats, exercises fixed and relocated metadata, rejects out-of-bounds metadata offsets, loads pages and checks that a session preserves time, pages, progress, title and author in the journal. Rapid turns retain their pages and accumulate subsecond dwell before rounding the session once. No sample books or user saves are required.

Touch delivery, physical display timing and real SD writes require the simulator or device checks in `docs/reading-stats.md`. This test does not replace those checks.
