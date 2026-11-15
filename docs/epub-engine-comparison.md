# Compare the EPUB engines / Comparar motores EPUB

[Download the comparison book](downloads/CrossNext-EPUB-Comparison.epub) or the shorter [reader showcase](downloads/CrossNext-Reader-Showcase.epub). Both EPUB files are English-only; this guide is bilingual.
It contains original English-only text, 240 numbered paragraphs in one chapter,
bold/italic text, headings, a quotation, a list, an internal link and PNG/JPEG
gray and line patterns. No font is embedded. Text and generated pictures: CC0 1.0.

## English

Use the same X4 Pro, microSD and book. Set font, point size, spacing, indentation,
alignment, margins, orientation, image settings and book styles identically.
Select **Settings → Reading → EPUB engine → New** or **Classic (1.4)**, then close
and reopen the book. Open chapter 2 and compare the same numbered paragraph;
page boundaries and total pages may differ between engines.

Measure first opening and repeated opening separately. Each engine has separate
page caches, and desktop preparation warms only the selected combinations.
Use a fresh copy of the book under a new filename to measure cold opening;
do not delete the whole `/.crosspoint/` folder, which also holds personal data.
After each setting change, wait for the page to settle and record the visible
response. Inspect images, thin lines, proportions and text quality as well as time.
The book makes no claim that an engine is faster or that all EPUB styles match.

## Español

Usa el mismo X4 Pro, microSD y libro. Iguala fuente, tamaño, interlineado, sangría,
alineación, márgenes, orientación, imágenes y estilo del libro. Selecciona
**Configuración → Lectura → Motor EPUB → Nuevo** o **Clásico (1.4)**, cierra y
vuelve a abrir el libro. En el capítulo 2 compara el mismo párrafo numerado:
los saltos de página y el total pueden cambiar entre motores.

Mide la primera apertura por separado de las siguientes. Los motores tienen
cachés distintas y el preparador solo adelanta las combinaciones seleccionadas.
Para una prueba en frío usa otra copia con un nombre de archivo nuevo. No borres
toda `/.crosspoint/`, porque también guarda tus datos personales.
Tras cambiar cada ajuste espera a que termine la página y anota el tiempo de
respuesta. Comprueba calidad del texto, tonos, líneas finas y proporción de imágenes.
El libro no demuestra por sí mismo una ventaja de velocidad ni igualdad de todos los estilos.

| Engine / Motor | Cold open / Frío | Warm open / Caché | Font / Fuente | Size / Tamaño | Spacing / Interlineado | Image / Imagen |
| --- | --- | --- | --- | --- | --- | --- |
| New / Nuevo | | | | | | |
| Classic / Clásico | | | | | | |

Rebuild deterministically with `python tools_local/make_comparison_epub.py`.
The generator validates ZIP CRCs, XML, manifest/spine paths and image files.
Those checks are not an EPUBCheck certification or physical-device performance test.
