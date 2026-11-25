# Cover-loading screen and checkbox feedback

Appearance > Cover loading notice includes a compact "Why enable it?" button at
the bottom right. It explains that Home appears sooner while covers load there.
Back returns to the choices without changing them. The translated footer adapts
to its text and follows the current theme's corners. See
[display uploads and help](display-upload-cover-help.md) for resource updates
and verification limits.

Changing a Cover loading notice choice updates its existing checkbox in place.
It no longer calls `resetUi()`: that method announces a new screen and arms the
paint-reveal gate, which can reject another touch during a same-screen repaint.
The gate remains in use for genuine screen entry. Saving, failure rollback and
the existing error message remain synchronous and unchanged. No pending-save
queue, resident buffer or additional setting is introduced.

Framed and Carousel keep the Continue reading badge and disabled arrows while
loading. Simple gray bars reserve title and author positions; Carousel and
Carousel2 show zero progress until book metadata is available. Bookshelf reserves
its grid titles and featured title/author/progress positions. The placeholders
use existing geometry and a handful of fills, not images, cached bitmaps or
animations. The legacy cover-grid shell also avoids preparing its image cache
before any book data is available.

Framed retains its existing wide horizontal loading notice. The normal GUI cover
placeholder is retained in every appearance. Only title/author/progress metadata
uses gray bars; provisional covers are not replaced with gray rectangles.

With a retained, current Bookshelf catalog, Home opens the book grid directly.
On any grid, available artwork is reused before trying to generate thumbnails.
When thumbnails need preparation, the grid displays available covers and
placeholders before preparing missing artwork during idle time. Home can reuse
a rendered placeholder for an EPUB confirmed to have no cover. A media or book
resource change invalidates the rendered scene.

Host checks execute the production CoverLoading toggle branch, both persistence
outcomes, actual Home draw functions and the production Bookshelf shell block.
They check title/author placeholders, zero progress, horizontal notice geometry and bounds
in portrait/landscape and English/Spanish. Shell tests also preserve entry phases
and a single first paint. Hardware: touch each choice once, verify its checkbox,
leave/reenter to check persistence; enable the notice separately for all four
appearances and enter Home with uncached covers. Confirm the provisional controls
do not activate a book before loading finishes. Check all four orientations and
Bluetooth/Wi-Fi afterward. Physical e-ink timing remains a device measurement.
