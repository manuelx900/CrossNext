# Carnivoro portraits

The user supplied the pixel portrait atlas used in `animals-atlas.png`.
Its SHA-256 is `dfeb5c77d87839a406458ebd4f9cdecbbea122cf4120f8ca17a33c8ecbeb50dd`.
It replaces the previous detailed woodcut artwork.

`tools_local/carnivoro_art.py` crops cell borders and converts the seven
portraits to 160×160 monochrome bitmaps with nearest-neighbour sampling and a
threshold of 155. It writes the individual PNGs and `CarnivoroArt.h`.
The firmware reads 22,400 constant bytes from flash, with no runtime image
decoder or SD art cache.
