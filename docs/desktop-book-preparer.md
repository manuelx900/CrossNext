# CrossNext Book Preparer 2.0.5

The desktop app runs the firmware's real preparation engine in WebAssembly with
the Node runtime bundled with Electron. See the [current 2.0.5 build and runtime instructions](../desktop-preparer/DESKTOP.txt).

## English

Orientation can follow the reader, use portrait, use landscape, or prepare both. Both prepares and verifies separate page layouts for each selected font and size, while counting the book once. It uses more card space; the warning counts both orientations. This does not change the saved reader orientation or the original EPUB. Rotating can reuse those pages while the engine, font, size, margins and other layout settings still match. XTC/XTCH pages are fixed; they do not need EPUB reflow.

Verified existing profiles are preserved when adding another orientation or font. Desktop certification version 5 distinguishes verified files from unrelated legacy layouts: a legacy audit records only the selected pages it actually checked, and unverified derived variants are retired when new output is published. Earlier desktop certifications rebuild once; reading positions and original books remain untouched.

The card summary counts complete books for the selected formats, fonts, sizes,
orientations and engine. For example, 300 of 2000 prepared books starts preparation at 300;
each successfully completed book adds one, regardless of its font/size passes.
Failed books remain separate. Changing the options recomputes the summary.
Legacy preparation markers are checked with the real page loader before the
first verified receipt is saved; later checks reuse unchanged receipt metadata.

The compact footer highlights the current book and chapter, keeps the overall counter beside the progress bar and aligns the preparation controls. Expand **Preparation details** to see which existing caches will be kept and why other books need preparation. Available card space shares the reader-settings row.

The summary explains how many verified books will be kept and how many remain. It separately reports outdated/incompatible preparations needing a cache update and books needing pages for the newly selected layout settings. Checking caches is identified before page generation begins; a zero count is not presented as unexplained loss of the previous work.

Diagnostics opens a paged report in the app with each failed book, a readable
reason and expandable technical details. The technical log folder remains
available from that report. Original books and reader state are not written.

An unfinished or failed book never publishes its generated files. A successful
book is verified before publication, then its new caches are staged on the card
with a recovery journal. Existing cache files are retained until publication
finishes. A handled failure restores them; after a process interruption, reconnect
the same card to Book Preparer to finish recovery before returning it to the reader.
This uses temporary disk space for one book, not extra firmware RAM. Prepared
cache files remain compatible with the existing reader.

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

Cancel leaves fully completed books intact; an unfinished book does not replace its previous caches. Resume by running the same selection again.
Books, settings, reading progress and statistics are preserved. Only known generated
cache paths under `/.crosspoint/` are written. Safely eject the card after completion.
Keep the diagnostic folder shown after an error. Do not remove the card during work.

The Windows executable is portable and unsigned. The macOS arm64/x64 DMGs are built and verified on macOS. They are not Apple-notarized; platform security prompts may apply. Open the matching DMG and drag the app to Applications. Version 2.0.5
and cache ABI versions are independent; `engine-build.json` records provenance.

## Español

Orientación permite usar la del lector, vertical, horizontal o ambas. Ambas prepara y verifica una paginación por orientación para cada fuente y tamaño, contando cada libro una sola vez. Ocupa más espacio; el aviso cuenta las dos orientaciones. No cambia la orientación guardada ni el EPUB original. Al girar se reutilizan esas páginas si coinciden motor, fuente, tamaño, márgenes y los demás ajustes de lectura. XTC y XTCH tienen páginas fijas y no necesitan redistribuir el texto.

Los perfiles ya verificados se conservan al añadir otra orientación o fuente. La certificación 5 del preparador distingue los archivos verificados de otras variantes antiguas: una revisión antigua certifica solo las páginas seleccionadas que comprobó, y las variantes derivadas sin verificar se retiran al publicar nuevos archivos. Las certificaciones anteriores se preparan de nuevo una vez; se conservan el progreso y los libros originales.

El resumen cuenta libros completos para los formatos, fuentes, tamaños, orientaciones y motor
seleccionados. Si hay 300 de 2000 preparados, la preparación continúa desde 300;
cada libro terminado suma uno, aunque tenga varias combinaciones de fuente y
tamaño. Los fallidos se muestran por separado. Cambiar las opciones recalcula
el resumen. Las marcas antiguas se comprueban con el cargador real de páginas
antes de guardar su primera verificación; las siguientes reutilizan sus metadatos.

El pie compacto destaca el libro y el capítulo actuales, mantiene el contador junto a la barra de progreso y alinea los controles de preparación. Abre **Detalles de preparación** para saber qué cachés se conservan y por qué otros libros necesitan preparación. El espacio disponible comparte fila con los ajustes del lector.

El resumen explica cuántos libros verificados se conservan y cuántos faltan. Distingue las preparaciones antiguas o incompatibles que necesitan actualizar la caché y los libros que requieren páginas con los ajustes seleccionados. La comprobación de cachés se identifica antes de generar páginas nuevas; un contador en cero no aparece sin explicar qué ocurrió con la preparación anterior.

El diagnóstico muestra una lista paginada de libros fallidos, sus motivos y los
detalles técnicos desplegables. Desde ahí se puede abrir la carpeta de registros.
No se escriben los libros originales ni el progreso del lector.

Un libro fallido o incompleto no sustituye las cachés anteriores. Cada libro
terminado se verifica antes de publicar sus archivos y usa un registro de
recuperación en la tarjeta. Si se interrumpe el proceso durante esa publicación,
vuelve a conectar la misma tarjeta a Book Preparer para restaurarla antes de
usarla en el lector. Se utiliza espacio temporal para un libro, sin añadir RAM
al firmware ni cambiar el formato de páginas que lee el dispositivo.

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

Windows es portátil y sin firmar. Los DMG para macOS arm64/x64 se generan y verifican en macOS. No tienen notarización de Apple; pueden aparecer avisos de seguridad. Abre el DMG correspondiente y arrastra la app a Aplicaciones. La versión 2.0.5 de la aplicación
y las versiones internas de caché son independientes; `engine-build.json` registra
el origen del motor. Comprueba las cachés con el firmware correspondiente en el X4 Pro físico.


The desktop engine uses the firmware's optional font modules, including auto-hinting. Older desktop caches are rebuilt once when their certification does not match the current font configuration. Incompatible generated variants for other font profiles are retired transactionally, so the reader cannot reuse them; original books and reader-owned state remain. Subsequent runs reuse matching complete books. The desktop image-cache row band can use up to 128 KiB to handle tall JPEG decode blocks; the device retains its existing 24 KiB limit and image quality. These changes add no firmware RAM allocation.

El motor del computador utiliza los mismos módulos de fuentes del firmware, incluido el ajuste automático de los trazos. Las cachés del preparador antiguo se vuelven a preparar una vez cuando su certificación no coincide con la configuración actual de fuentes. Las variantes generadas incompatibles de otros perfiles se retiran durante la actualización recuperable para que el lector no las reutilice; se conservan los libros originales y los datos del lector. Las siguientes ejecuciones reutilizan los libros completos compatibles. La banda de caché de imágenes puede usar hasta 128 KiB en el computador para procesar bloques JPEG altos; el lector conserva su límite de 24 KiB y la calidad de imagen. Esto no añade asignaciones de RAM al firmware.
