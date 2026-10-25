# stb_image 2.30

Vendored unchanged from https://github.com/nothings/stb/blob/master/stb_image.h
on 2026-10-21. SHA-256:
`594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`.
The upstream MIT/public-domain license text is retained at the end of the header.
CrossNext uses the MIT option. Keep that notice when redistributing the source.

Only JPEG is compiled, without stdio, SIMD, HDR or linear-float conversion.
`FlashJpegDetail.cpp` owns fallible PSRAM-only allocation with a 6 MiB total cap.
This decoder runs only during progressive-JPEG HD import or explicit legacy Zoom
recovery. Ordinary previews use the existing decoder; panning decodes nothing.
Source bounds remain 2048 x 3072. Images that exceed available PSRAM retain the
existing reduced decoder fallback. No network or extra task stays active.

Full-detail JPEG input must end in its EOI marker. Incomplete or trailing-data
files retain the existing decoder fallback; they are not published as full-detail data.
