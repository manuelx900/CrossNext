# Statistics book artwork

`Book.png` is the transparent PNG created with the built-in imagegen tool for
the individual book's statistics heading. Its balanced open-book design uses
firm, consistent outlines. It replaces the File Browser icon in that heading
only. The source alpha is preserved.

Run `python tools_local/navigation_art/generate_statistics.py` to regenerate
`src/apps_local/ui/StatisticsIcons.h`. The shared artwork packer composites
over white, crops the ink bounds and centres the book with four pixels of total
inset. It packs one-bit masks at the actual 54-pixel landscape and 80-pixel
portrait display sizes. These masks occupy 1,178 bytes of flash; no PNG decoder,
new heap buffer or runtime image preprocessing is needed on the device.
