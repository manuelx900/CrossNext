# Sleep wake and statistics pages

## Wake before the first screen

The reported delay is while the sleep image is still visible. Startup used to
load all four bundled Noto Serif sizes, as well as all three language fallback
faces, before routing Home. The previous registry discovery improvement did
not remove these loads. This change retains eager preparation of the selected
reader family. Noto Serif loads its active size first; the other fixed content
sizes are requested by their tools or the existing font-variant provider.
The tools are Flashcards, Words, Study, Wikipedia, dictionary definitions and
the reader text sheet. They retain their original faces and metrics.

Language fonts register their three fallback IDs at boot, then load a face only
when the text-resolution path or keyboard coverage check needs its characters.
This happens before measuring/drawing, through the existing font provider under
the renderer's ownership. Missing fonts and OOM are attempted once per size
rather than once per label. Regional SC/TC and generic fallback behavior is
preserved. Latin text already covered by a primary face does no resource I/O.
No panel waveform, reset timing, sleep mode, active vector-reader glyph cache,
EPUB pagination format or radio policy changes.

The bundled content hash still folds metadata in the original order
14, 12, 16, 18. The probe uses bounded stack scratch to check style/count/offset bounds,
bitmap tails and streamed coverage intervals before publishing that identity.
A damaged inactive size rejects the bundled family, so an earlier cached layout
cannot silently draw its headings with fallback metrics. Valid resources produce
the same hash as the former four loaded faces. Lazy loading an additional size does not invalidate EPUB caches.
A registry refresh clears the size ladder and aggregate hash before reloading.
The tests check the actual four resource headers and selected-size load counts;
these are work counts, not physical wake timings. Integrity checks add reads
proportional to the deferred interval tables, without allocating font objects or
coverage tables. Each file probe still uses the existing small HAL file handle.

## Prepared statistics lists

The day/month/year book lists sort by accumulated seconds descending, with
stable document IDs breaking equal-time ties. An individual book's dates sort
only by date descending. The minimum visible duration remains 60 accumulated
seconds per row; all original sessions remain in the journal.

Previously every arrow scanned the complete history again, sometimes multiple
times, and presented a separate loading screen. A list is now prepared once
from one journal pass. Equal keys in each eight-contribution input batch
are combined before writing, without applying the one-minute filter early.
Bounded stable merge passes and a grouping pass on SD
produce a disposable snapshot. Further pages read at most five 256-byte rows
with one seek/read, know the exact total, and request only their final repaint.
The common firmware paginator displays current / total and draws both arrows,
including one-page or empty lists. Portrait uses five rows and landscape two.

The two activity-owned views use only the existing ten Book work slots and
3,072-byte I/O buffer. During collection the journal keeps its read-ahead; an
inactive Book slot holds the small row codec. During merging the same I/O
buffer serves two 1 KiB input windows and one 1 KiB output window. No container
scales with the history. Work is cooperative: bounded collection batches and
up to eight output rows per merge step; the existing activity loop yields
without its usual inter-step sleep while there is work.

Temporary files are `/.crosspoint/stats-period-a.tmp`, `stats-period-b.tmp`,
`stats-days-a.tmp` and `stats-days-b.tmp`. They are derived data, require writable
card space, and are closed/deleted on normal exit. Failure paths close their handles and
attempt deletion; any undeletable leftovers are ignored/recreated next time. They can be recreated
after an interrupted build; they are never treated as saved reading history.
Peak temporary SD storage is approximately twice 256 bytes per relevant
session contribution (a multi-day session contributes one row per date), plus
any already prepared other view. An unreadable, full or write-protected card
shows the existing storage-error view with Back; original history is untouched.
A period snapshot remains available when a book's date snapshot is prepared.

## Verification

`host-tests/wake-fonts/run.py` checks real font header hashes, selected-size
loading, fixed-ID guards, lazy fallback routing, regional selection, missing
resources/OOM and release. `host-tests/stats-view/run.py` exercises production
merge/group/page I/O and production activity page actions against ranked and
date reference totals, including midnight splits, sub-minute sums, different
capacities, reverse navigation, simultaneous views and storage failures.
The existing statistics raster suite checks exact current / total labels and
touch geometry across orientations, themes, font styles and languages.

On the X4 Pro, compare at least five sleep wakes with the same card, sleep image,
active reader font and theme as the previous BIN. Measure until Home begins
changing and until it responds; also test wake directly into a cached EPUB.
Try a vector family and Noto Serif at each size, headings/italics/bold, a font
size change, dictionary/text-sheet use and opening the named content tools.
Check non-Latin titles, Chinese SC/TC and an absent language-font file on a
backup card. Existing cached EPUBs should retain their identity and warm page
speed. Compare internal heap, PSRAM and largest blocks during repeated cycles,
Bluetooth paging and Wi-Fi transfers. These require physical measurements.

For statistics, use more than five visible books and dates with different
minutes: check descending book totals versus descending dates, current / total,
both arrows, rotations and Back through book details to the original period.
After initial preparation, arrows must display the next list directly without
the history-reading notice. Exit during preparation and verify later entry
works. On a backup card, simulate a storage error; original sessions/settings
must remain readable and Back must exit. No device timing/RAM peak or native
panel verification is claimed by host tests.
