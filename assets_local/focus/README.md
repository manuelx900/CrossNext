# Focus menu illustration

`pomodoro-source.png` is the pixel-art Pomodoro supplied by the user on 2026-11-05. It is retained as an unused source; the Focus home no longer draws an illustration.

Run `python tools_local/focus/build_pomodoro.py` (Pillow required) to generate `pomodoro.png` and `src/apps_local/focus/FocusPomodoroArt.h`. The encoder crops the empty paper margin, preserves the square proportions, cleans near-white/near-black noise, resizes with nearest-neighbor sampling and dithers the gray details to one bit. The 230 × 230 bitmap contains 6,670 bytes of constexpr flash data (29 bytes per row) and adds no runtime allocation or SD resource. Its portrait size is approximately 20% larger than the original 192-pixel preview.

The older book illustrations below are retained as unused sources and provenance.

`books-source.png` is original Focus artwork generated with the built-in imagegen tool: an open book with a ribbon bookmark atop two closed books, detailed black pen engraving with hatching and stippling, isolated on white, no text, room or characters, compact 8:5 composition. It is independent of the Pet album illustration.

Run `python tools_local/focus/build_books.py` (Pillow required) to produce `books.png` and `src/apps_local/focus/FocusBooks.h`. The encoder fits it to 256 × 160, dithers to one bit and packs 5,120 bytes of constexpr BW1 flash data. Rendering requires no runtime allocation or SD resource.

The older, unused bookcase source is retained below for provenance.

`bookshelf-source.png` was generated with the built-in imagegen tool for this redesign. It is an original bookcase illustration; the reference screenshots supplied composition and visual direction.

Prompt: A front-on, wide wooden bookcase with two low shelves, varied upright and leaning books and small horizontal stacks. Black ink woodcut / pen engraving, sparse stippling, isolated pure white background. No text, people, scenery, gradients or room. Banner ratio 3:1, readable at 420 × 180 pixels.

Regenerate the packed resource with `python tools_local/focus/build_bookshelf.py` (Pillow required). It resizes to 432 × 144 and thresholds at 155. The generated header contains 7,776 bytes of constexpr bitmap data in flash and requires no runtime allocation or SD resource.
