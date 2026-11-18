# CrossNext Book Preparer 2.0.3

The desktop app runs the firmware's real preparation engine in WebAssembly with
an included Node runtime. See the [current 2.0.3 build and runtime instructions](../desktop-preparer/DESKTOP.txt).

## English

Select the microSD root. The app detects the saved reading engine, font, size,
orientation, layout and XTC cover rotation. Interface and reading fonts are separate.
Choose EPUB, XTC and/or XTCH. Vector fonts support 8–22 pt; bitmap families use
their installed sizes. Reader size selects the nearest supported size per family;
all sizes prepares each family's supported sizes. Font selection affects EPUB only.

**Start with one font and one size.** Every EPUB font/size combination stores a separate pagination for every book. Selecting all combinations can fill the card, especially when its allocation blocks are large. Before preparation, the app shows the combination count, free space and an explicit confirmation. You can return to the reader's font and size without starting preparation. Final space cannot be predicted exactly from the book sizes alone.

**Clear CrossPoint cache** is available beside the card's free space. Choose **Prepared pages by date** to remove layout variants last modified within an inclusive local-date range, preserving earlier page files and shared book metadata. Choose **All book caches** to remove recognized generated pages, covers, decoded images and indexes. Unknown files, books, settings, reading progress, vocabulary, statistics and game saves are never selected. macOS `._` files are included only when their names correspond to recognized generated caches.

Review the estimated space and file count, read the warning and tick the acknowledgment to enable deletion, then confirm separately. Reviewing alone does not change the card. Deletion cannot be undone, and a date filter does not restore older overwritten versions. Deleted layouts will need preparation again. Validation markers for the affected books are invalidated before deleting pages; shared caches are retained in date mode to avoid invalidating older layouts. Files changed after review are skipped, and linked cache folders are rejected. Stop leaves the files already deleted removed and the remaining files intact.

Preparation checks available space before it starts and before each changed output is copied to the card. It preserves a reserve of 1% of card capacity, bounded to 128–512 MiB; when space is low, preparation stops with an explanation instead of deliberately consuming the reserve. Concurrent writes by other programs can still exhaust a card, so keep other card-writing applications closed. Cleanup plans are streamed to the computer's temporary directory and do not need free space on the card. These changes run on the computer and add no firmware RAM usage or change to EPUB rendering quality.

Select **New** or **Classic** to match the reader. Changing it in the desktop app
does not change the reader's setting. It prepares only that engine's pages.
Changing layout later can require new pages. Covers include the dimensions used
by all four appearances and Library. Valid existing variants are reused.

Cancel leaves completed outputs intact. Resume by running the same selection again.
Books, settings, reading progress and statistics are preserved. Only known generated
cache paths under `/.crosspoint/` are written. Safely eject the card after completion.
Keep the diagnostic folder shown after an error. Do not remove the card during work.

The Windows executable is portable and unsigned. The macOS arm64/x64 DMGs are built and verified on macOS. They are not Apple-notarized; platform security prompts may apply. Open the matching DMG and drag the app to Applications. Version 2.0.3
and cache ABI versions are independent; `engine-build.json` records provenance.

## Español

Selecciona la raíz de la microSD. El programa detecta motor, fuente, tamaño,
orientación, diseño y giro de portadas XTC guardados en el lector. La fuente de
interfaz es independiente. Elige EPUB, XTC y/o XTCH. Las fuentes vectoriales
admiten 8–22 pt y las bitmap sus tamaños instalados. «Tamaño del lector» toma el
compatible más cercano de cada familia; «Todos los tamaños» prepara los de cada
una. Las fuentes solo afectan a EPUB.

**Empieza con una fuente y un tamaño.** Cada combinación genera una paginación aparte para cada EPUB. Elegirlas todas puede llenar la microSD, especialmente si utiliza bloques de asignación grandes. Antes de preparar se muestran las combinaciones, el espacio disponible y una confirmación. Puedes volver a la fuente y tamaño del lector sin iniciar la preparación. El espacio final no se puede calcular exactamente a partir del tamaño de los libros.

**Eliminar caché de CrossPoint** aparece junto al espacio disponible de la tarjeta. **Páginas preparadas por fecha** elimina las variantes cuya última modificación corresponda al periodo elegido, con ambas fechas incluidas y según la hora local. Conserva las páginas anteriores y los metadatos compartidos del libro. **Toda la caché de libros** elimina páginas, portadas, imágenes e índices generados reconocidos. Se conservan archivos desconocidos, libros, ajustes, progreso, palabras guardadas, estadísticas y partidas. Solo se incluyen archivos `._` de macOS si corresponden a cachés generadas reconocidas.

Primero revisa el espacio estimado y la cantidad de archivos; lee el aviso y marca la casilla para habilitar la eliminación, y después confirma. Revisar no modifica la tarjeta. Borrar no se puede deshacer y el filtro por fecha no recupera versiones anteriores sobrescritas. Las páginas borradas tendrán que prepararse otra vez. Antes de borrar páginas se invalidan sus marcas de preparación, mientras que el modo por fecha conserva los recursos compartidos para no invalidar las páginas anteriores. Se omiten los archivos modificados después de la revisión y se rechazan carpetas enlazadas. Si detienes la limpieza, lo ya eliminado no se restaura y el resto se conserva.

La preparación comprueba el espacio al comenzar y antes de copiar cada archivo modificado. Deja una reserva del 1% de la tarjeta, con un mínimo de 128 MiB y un máximo de 512 MiB. Si falta espacio, se detiene con un aviso. Otro programa que escriba simultáneamente puede agotar el espacio, por lo que conviene cerrarlo. El plan de limpieza se guarda temporalmente en el computador y no necesita espacio libre en la tarjeta. Estos cambios no consumen RAM del firmware ni alteran la calidad del EPUB.

Selecciona el mismo motor **Nuevo** o **Clásico** que en el lector. Cambiar el
selector del ordenador no cambia los ajustes del dispositivo. Solo prepara las
páginas de ese motor. Otro diseño puede necesitar otra paginación. Se preparan
los tamaños de portada de las cuatro apariencias y Biblioteca, reutilizando los válidos.

Cancelar conserva lo terminado y puedes continuar con la misma selección.
Solo se escriben rutas conocidas de caché generada en `/.crosspoint/`; se conservan
libros, ajustes, progreso y estadísticas. Expulsa la tarjeta al terminar y conserva
la carpeta de diagnóstico si falla. No la retires durante el proceso.

Windows es portátil y sin firmar. Los DMG para macOS arm64/x64 se generan y verifican en macOS. No tienen notarización de Apple; pueden aparecer avisos de seguridad. Abre el DMG correspondiente y arrastra la app a Aplicaciones. La versión 2.0.3 de la aplicación
y las versiones internas de caché son independientes; `engine-build.json` registra
el origen del motor. Comprueba las cachés con el firmware correspondiente en el X4 Pro físico.
