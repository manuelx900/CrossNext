# Importing CrossInk statistics

Open **Settings → Library → Import CrossInk statistics**. Refresh Library first if requested.
The importer reads the original microSD's `/.crosspoint/global_stats.bin`, or a copy at
`/crossink-import/.crosspoint/global_stats.bin` (the explicit copy takes priority).
Copy the original `epub_*`, `xtc_*`, and `txt_*` cache folders alongside that file and
keep books at their original paths. Source files are never renamed, rewritten, or removed.

CrossInk global versions 1–3 and per-book versions 1–5 are supported. A valid backup
is tried when a primary file is absent or invalid. Book lookup supports FNV path cache
identities and legacy platform `std::hash` identities. The latter requires the same hash
implementation; unmatched books still contribute to the global totals.

Importing again **replaces** the imported snapshot, rather than adding it. The local
CrossNext session journal and CrossPoint Sync snapshots are independent and untouched.
Two checksum-validated snapshot slots preserve the previous successful import if a
write fails or is interrupted. Cancellation discards the in-progress copy. The import
uses a bounded, fallible PSRAM array (at most the Library's 4096 books); there is no
internal-RAM fallback, background network task, or second framebuffer.

Statistics shows imported CrossInk history plus local CrossNext reading by default
when an import exists. Use the history button to choose local-only, imported CrossInk,
or CrossPoint Sync. Different historical sources are **not** added together.
Do not import the same reading a second time through a different local journal.

## Limits of the original data

CrossInk stores lifetime sessions, time, page turns, completed-book totals, per-book
accumulators, and (version 3) activity flags for the last 730 days. An activity flag
does not say how many minutes were read. The import does not invent dated minutes,
individual sessions, goal completions, or rewards for PET/Reading Quest. CrossInk
time-of-day buckets have different boundaries and are not relabelled as CrossNext buckets.
Peer snapshots from `synced_stats` are not automatically summed: they can include
duplicates and do not identify matching book histories. Keep those originals as well.
Deleted or moved books may have lifetime contributions but no matched detail row.
The current Library must contain a book for its per-book cache to be matched.

Source formats were checked against [CrossInk source](https://github.com/uxjulia/CrossInk/tree/9914146eeae7b46b300f475a16c32426fc02ec1f/src/activities/reader).

## Verify on X4 Pro

1. Preserve a copy of the original `.crosspoint` folder and note CrossInk totals.
2. Refresh Library, run the importer, then compare totals and matched book details.
3. Run it again: imported totals must stay unchanged. New local reading is added once.
4. Verify Library, reading progress, local history, PET and goal records remain unchanged.
5. Try both orientations. Cancel halfway and confirm the previous import still opens.

No physical-device verification is claimed by the automated tests.
