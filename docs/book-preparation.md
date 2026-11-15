# Prepare books

Open **Settings → Library → Maintenance → Prepare books**. The **Prepare full
book on opening** switch is immediately below this action. That optional switch
keeps its existing compact first-open progress popup; it uses the book’s current
reading configuration.

The manual preparation screen lets you select EPUB, XTC and XTCH independently.
All three are selected initially. For EPUB, choose one or more installed reading
fonts, then a point size or **All available sizes**. Vector fonts offer the same
8–22 pt sizes as the reader; bitmap fonts use their installed sizes. With one
size selected, each font uses its nearest available size, rounding ties down.
All sizes prepares each family’s available sizes, rather than a union of sizes
that a particular family may not support. Preparing books does not change your
saved reading font or point size.

Review the formats, selected EPUB engine and number of page layouts before
starting. A large library, multiple fonts or all sizes can take many hours and
consume substantial microSD space. Keep power connected. Cancel preserves
completed page caches; starting the same selection again reuses them.

EPUB pages are prepared only for the engine selected when this screen opens.
Choose the other engine in Reading settings to prepare its separate caches.
Orientation, margins, spacing and other layout changes can also require a new
layout. XTC and XTCH already contain fixed pages: they need no font pagination.
When only those formats are selected, preparation skips font loading and runs
one cover pass. All selected formats get the thumbnail geometries used by all
home themes, library views and orientations, with the configured XTC rotation.
Missing source covers still use the ordinary book placeholder.

The work queue stays on microSD. One font, book and incremental chapter builder
are active at a time. A fixed family/size snapshot is retained by the activity;
there is no per-book RAM array. Completed EPUB layouts are persistent prepared
files independent of the reader’s four ordinary recently used layout slots.
Progress counts use 64 bits for large font/size/library combinations. Font and
size lists support horizontal swipes and pagination in both orientations.

On a device, test a mixed EPUB/XTC/XTCH folder, prepare each format separately,
then one EPUB font at two sizes and All available sizes. Reopen at each prepared
size with the same engine and layout settings. Return to each home theme to
check its covers. Cancel and resume once; verify the reading font preference did
not change and that the first-open switch appears directly below Prepare books.
