# Books read in a period

The book button in Today's reading-time card and a selected calendar day, week,
month or year opens the books read during that interval. In the annual Stats
view it sits at the lower right of the total-time card; the upper controls still
choose the year and calendar period. The all-time view has no annual button.

Each row shows only the book title and minutes recorded in the selected interval.
Titles use at most two lines, ending in an ellipsis when they still do not fit.
The title uses the normal Books-list interface cut. The minutes column grows for long
values and the existing numeric renderer fits the complete value to its bounds.
Opening a row uses the existing lifetime book-statistics detail. Back returns to
the same list page, then to the original day/month/year. Orientation changes
restart the list at page one with two landscape or five portrait rows.

The journal stores individual local sessions. Sessions crossing midnight or a
month/year boundary contribute only their overlapping seconds. Completion and
reopening markers do not add reading time. Time is summed in seconds before
the existing minute presentation rounds down. A book or date row appears only
after its accumulated time reaches 60 seconds in that list's interval. Two
sessions of 35 and 30 seconds qualify; a total of 59 seconds does not. The same
minimum applies to the normal Books lists and direct book detail. Raw sessions,
seconds, progress and aggregate statistics remain stored and counted internally.
The aggregate card can therefore include seconds from books hidden in the list.

Imported CrossInk/cloud snapshots can contain daily totals and lifetime book
totals without a book-by-day breakdown. The period list reads the authoritative
local journal and says **Only this device** in a combined-history view. It does
not infer a remote book's dates or divide its lifetime minutes across dates.
Opening the existing detail still follows the selected local book's journal.

## Bounded work and ownership

Lists rank books by exact interval seconds, greatest first; document ID breaks
ties consistently. `PagedView` prepares a disposable SD snapshot from one journal
pass. Bounded stable merges group complete document totals before filtering the
60-second minimum, then rank the aggregate rows. Only the existing ten inactive
Recent slots and 3 KiB read-ahead buffer are borrowed; no history-sized RAM
container, task or framebuffer is added. Recent is invalidated and rebuilt if
requested later.

Later pages use one seek/read of at most five 256-byte prepared rows, without
another journal scan or a loading-screen transition. The exact row total is
known before the first page is presented. Work during preparation is bounded
and cooperative, and Back cancels it. Two temporary files per view are owned by
the activity and removed on exit/failure. The card must have writable temporary
space. No journal or saved reading data is changed. See
[wake and paginated statistics](wake-stats-pages.md) for the complete ownership,
storage cost, failure behavior and device checks. Radio ownership is unchanged.

## Dates for one book

The calendar button at the upper right of a book's total-time card opens its
recorded dates and minutes, newest first. Back returns to that book's existing
detail; another Back follows the original navigation, including returning to a
period's book list. Date rows are informational, with no misleading row arrow.

`BookDayPage` supplies the small visible date/seconds array. A separate prepared
view groups all session contributions by local date and sorts only by descending
date, never by minutes. Midnight-spanning sessions are split cooperatively;
sub-minute contributions are summed before filtering. Its workspace is shared
with period preparation, while the prepared period file remains available for
Back navigation. Combined-history date lists still say **Only this device**:
remote lifetime totals cannot be assigned to dates that were not imported.

Both lists use the firmware's shared pager drawing. Both arrows are always
visible, including empty and one-page lists; only available directions act.
The centre shows current / total, including 1 / 1 for an empty or one-page list. Returning from a book's date list preserves
the containing period list without rescanning it.

The pager sits 12 pixels above its former position. The date list uses the
regular interface size for its book heading and dates, with measured title
height and a separate bottom margin. Time values remain centred on the entire
card, with symmetric space around the value when corner actions are present.
Book detail uses an outlined open book generated with imagegen and packed into
flash masks at its two display sizes. The smaller list icons still use the
File Browser's EPUB artwork. Source and regeneration instructions live in
`assets_local/statistics-icons/`.

The local finished-book index tracks the latest qualifying completed snapshot,
including a later session that takes an already completed book past one minute.
Reopening removes that book from the finished list. Internal completion totals
are unchanged. Combined-history Books lists count qualifying merged books in
cooperative batches on their first visit and cache those counts during the
activity's lifetime. They reuse the existing record and page buffers.

## Verification

Run `host-tests/period-books/run.py` with the repository Python environment. It
compares bounded pagination with a full reference aggregation over shuffled
sessions, checks boundary splits and renders populated production layouts in
portrait/landscape, light/dark chrome, both interface-font modes, English/Spanish.

On X4 Pro, read two books today and open the day, month and year lists. Their
minutes must reflect that period, and a book must open its existing detail.
Check next/previous pages, both Back controls, empty dates, orientation changes
and combined-history labelling. With Bluetooth connected, turn pages before and
after visiting Stats; confirm the radio stays connected. Serial heap/largest
block measurements are still needed for the physical peak budget.

Check a new book after 59 seconds: it must not have a book/date row. After
reaching 60 accumulated seconds it must appear as 1 minute. Reopening the book
for several short sessions must still accumulate their seconds. Test lists
with short visits between qualifying books/dates: pages must fill without
zero-minute rows or an empty next page.

Also open a book's date list, page both ways and return to its detail, including
an empty book and a book opened directly from the reader. Check minute values
above 999 and at least six digits: neither digits nor units may overlap the
title, divider or next-row control. Physical/remote Confirm on a period list
must open the selected book, not change pages.
