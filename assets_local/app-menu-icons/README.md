# Apps menu artwork

These PNGs are the supplied drawings for the 15 Apps rows. To regenerate
`src/apps_local/ui/AppMenuIcons.h`, run
`python tools_local/navigation_art/generate_apps.py` from the repository root.

The generator uses the same 40-pixel canvas and 34-pixel artwork area as the
Games menu. It preserves each drawing's proportions and packs it into a
monochrome mask at build preparation time.

`Quotes.png` is cropped to exclude the detached line above the book.
