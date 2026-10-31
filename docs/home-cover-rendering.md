# Home cover rendering

Framed and Carousel Home continue to use the existing monochrome cover files.
This change does not regenerate old thumbnails, read a book's original image to
upgrade it, change the layout, or add a screen refresh. Detail already discarded
when a thumbnail was created cannot be recovered from that file.

Monochrome rows are read in their packed form. When reducing a larger image or
compressing a carousel side cover, Home averages the covered source pixels before
converting the result back to black and white. This reduces aliasing of an
already dithered image. Images at their native size retain their existing pixels;
enlargements retain the existing point-sampling behavior. Other BMP depths keep
their previous rendering path.

Home can reuse three rendered art regions across activity changes. The single
arena holds three regions of up to 48 KiB each, plus metadata (about 152 KiB of PSRAM), with no internal-RAM fallback, no new
task and no SD writes. It contains cover art, not a second display framebuffer;
Bookshelf also retains shelf titles and progress badges, keyed by content, font
and language. The featured-book text/progress, menu and status redraw normally.
Region size, orientation, appearance, night mode and source paths form the cache
identity. Each reuse checks the image files' sizes and modification dates.
File-transfer and book-cache mutation callbacks also invalidate retained scenes
through an atomic revision, including same-size replacements with synthetic FAT
dates. The next Home render captures current art again without preparing images.
Missing, changed or undated files and oversized regions use the ordinary drawing
path. If the arena cannot be allocated, Home continues without it. The arena
survives activity changes and is released at firmware shutdown; it is not stored
on the SD card.

Framed and Carousel both cycle through the three most recent available books.
This Home limit does not truncate the existing recent-book store or its Library view.

The existing reusable row allocation also owns the reduction scratch. On X4 Pro
its maximum supported-row budget remains 12,800 bytes; if reduction scratch is
unavailable, Home falls back to point sampling before using the existing image
error handling. No reader, dictionary, wallpaper or thumbnail format is changed.

Before distribution, compare first entry, repeated returns from Files, Apps and
the reader, and carousel navigation on an X4 Pro. Check both appearances, all
four orientations and night mode. Confirm cover updates and cache clearing do
not retain old images; text and progress must still update. Simulator measurements
describe the host renderer and storage-call counts, not physical SD or panel
latency. No firmware release or hardware performance result is implied by this
document.
