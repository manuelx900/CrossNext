CURRENT DESKTOP VERSION: see DESKTOP.txt for the redesigned Windows/macOS UI,
downloads, usage and build instructions. The text below documents the legacy
WinForms preview, retained for reference.

CrossNext Book Preparer — Windows x64 portable preview (legacy)

ENGLISH
Run CrossNext-Book-Preparer-DPI.exe. English opens by default; select Español
in the top-right language menu to switch to Spanish. No installation is needed.
The title bar is black, with the white CrossNext logo. Drag its empty area to
move the window, double-click it to maximize, or resize from the outer edges.
Text and controls follow Windows display scaling, including 200%. The window
opens at a compact size within the monitor's work area. Base text follows the
Windows message font size; narrow windows stack the options vertically.
Scroll the settings area on smaller screens. Progress and actions stay visible.

1. Connect the microSD directly to the computer, or expose it through the
   reader USB-storage mode. Close other applications that write to the card.
2. Select the card ROOT (for example E:\), not an individual books folder.
3. The application reads /.crosspoint/settings.json, scans /fonts and /.fonts,
   and selects the saved reading font and point size. The interface font is
   independent. A missing reading font is reported; Ubuntu is the fallback.
   The list is scrollable and uses the same font discovery as the firmware,
   including custom TTF/OTF/TTC and compatible .cpfont families (up to the
   reader's 128 SD-family limit). Refresh rescans settings and installed fonts.
4. The EPUB engine selector starts with the reader's saved engine (Modern or
   Classic). You can change it for this preparation without modifying the
   reader's settings. Select the same engine on the reader to use those pages.
5. Choose EPUB, XTC and/or XTCH. Select more fonts if desired. Reader size uses
   the nearest supported size of each selected family; All supported sizes
   prepares every size each family supports. Explicit sizes are shared sizes
   supported by all selected families. TTF/OTF supports 8-22 pt, .cpfont uses
   installed sizes, and the resident Ubuntu fallback offers 12 pt. Unsupported
   sizes are rejected before preparing books. XTC/XTCH pages are fixed images, so
   font and size choices affect EPUB pagination only.
6. Prepare books. The book name, font, size, chapter and progress appear while
   it runs. Large libraries, several fonts and all sizes can take hours.
7. Cancel preserves valid completed outputs. Run again to continue, reusing
   existing valid caches. The counter includes only books still needing the
   selected engine, fonts, sizes or covers; already prepared books are omitted.
   Safely eject the card in Windows when finished.

The saved orientation, reading layout and XTC-cover rotation are used.
Cache geometry uses the X4 Pro hardware profile, including its measured bezel
insets. Existing prepared files are reused; missing or incompatible variants
are generated without clearing the library's other valid caches.
Only the chosen EPUB engine and layouts are prepared.
Changing font, size, orientation, engine or layout later can require different
pagination. Replacing books/fonts or changing firmware cache formats may also
require regeneration. Prepared files are not guaranteed for arbitrary firmware
versions. This preview targets the current CrossNext 2.0.0 preparation code.

The engine is the actual firmware parser, renderer and preparation pipeline,
compiled to 32-bit WebAssembly for the included Node runtime. No separate
approximate paginator is used. It prepares book metadata, pages, processed
images and the cover variants used by the appearances. Writes go only to
known generated-cache files under /.crosspoint. Books, fonts, settings,
reading progress and statistics are preserved. Temporary work files and the
unpacked runtime use Windows temporary/local application-data folders.

Known limit: host round-trip tests validate the generated page format using
CrossNext's reader loader. Cache consumption and performance still need a
physical X4 Pro check with this preview and its matching firmware. Device
waveform/display behavior is not simulated by this executable.

If an operation fails, its diagnostic folder is shown in the window. Keep it
when reporting an issue. Do not remove the card while preparation is running.
Runtime licenses and build provenance are embedded and unpacked alongside the
runtime under %LOCALAPPDATA%/CrossNext/Preparer. Source build instructions are
in BUILD.txt. The executable is unsigned.

ESPAÑOL
Abre CrossNext-Book-Preparer-DPI.exe y selecciona Español arriba a la derecha.
Es portátil: no necesita instalación. Puedes ampliar la ventana con el botón
superior, moverla arrastrando la barra negra o ajustar sus bordes.
El texto y los controles siguen la escala de Windows, incluido el 200 %. La
ventana abre con un tamaño compacto y toma como referencia la letra de Windows.
Las opciones se apilan cuando hay poco ancho.
En pantallas pequeñas puedes desplazar los ajustes; el progreso y los botones
permanecen visibles. El botón negro conserva el texto blanco durante el proceso.

Selecciona la raíz de la microSD, por ejemplo E:\. El programa lee los ajustes
del lector y detecta las fuentes de /fonts y /.fonts. Marca automáticamente
la fuente de lectura guardada y su tamaño. La fuente de la interfaz es
independiente. Si falta la fuente, lo avisa y ofrece Ubuntu como respaldo.
Actualizar vuelve a detectar los ajustes y las fuentes de esa tarjeta.

Selecciona formatos, fuentes y tamaños. «Tamaño del lector» usa el tamaño
compatible más cercano de cada fuente; «Todos los tamaños disponibles»
prepara todos los que admite cada una. Los tamaños individuales mostrados
son compatibles con todas las fuentes seleccionadas. En XTC/XTCH las páginas
son imágenes fijas: las fuentes y tamaños solo afectan a la paginación EPUB.
Las TTF/OTF admiten de 8 a 22 pt; las .cpfont ofrecen solo los tamaños
instalados. El programa muestra los tamaños admitidos y rechaza los demás.

El selector de motor EPUB comienza con el motor guardado en el lector. Puedes
elegir Nuevo o Clásico para esta preparación sin cambiar sus ajustes. Selecciona
ese mismo motor en el lector para aprovechar las páginas preparadas.
Usa la orientación, diseño y giro de portadas XTC guardados en el lector.
Prepara páginas, imágenes procesadas y portadas de las apariencias.
Usa las medidas reales del X4 Pro, incluidos sus márgenes de pantalla.
Al reanudar, el contador incluye solo los libros pendientes para el motor,
las fuentes, tamaños y portadas elegidos; omite los que ya están preparados.
Conserva las cachés válidas y completa las variantes que falten.
Cambiar después estas opciones, sustituir un libro/fuente o actualizar el
formato de caché puede necesitar una preparación nueva. Las bibliotecas
grandes con varias fuentes y tamaños pueden tardar horas.

Cancelar conserva lo ya preparado correctamente. Ejecuta de nuevo para
continuar. Los libros, ajustes, progreso y estadísticas se conservan; solo se
escriben archivos de caché generada dentro de /.crosspoint. Expulsa la tarjeta
con Windows antes de retirarla. Si hay un error, guarda la carpeta de
diagnóstico indicada en pantalla. Esta versión de prueba aún requiere
comprobar físicamente las cachés en un X4 Pro con el firmware correspondiente.
