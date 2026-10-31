# CrossNext v1.4.0 · Xteink X4 Pro

## English

### Home and interface
- Two new Home themes: **Carousel 2**, with large book covers and six shortcuts, and **Bookshelf**, with a grid of covers, reading progress and a side menu.
- Choose the Home theme and the interface theme independently, including overrides by section. Framed and Carousel keep their square boxes; Carousel 2 and Bookshelf use light headers and rounded controls.
- More consistent typography, spacing, icons and controls in English and Spanish, in portrait and landscape. Noto Sans remains the default. Ubuntu and Noto Serif remain available; Jersey 25 is removed and saved Jersey preferences fall back to Noto Sans. Book fonts are independent.
- Home supports up to seven shortcuts. Bookshelf's Appearance option controls its number of pages; the other Home themes use the recent-book count.

### Library and reading
- **Recent / Filter** navigation: filter by genre, series, author first or last name, or the first letter of the title, then choose a value to see matching books. Series retain volume order when metadata includes it.
- Recent retains up to 30 opened books, with nine rows per portrait page, titles wrapping onto two lines, and a list/cover switch. Direction arrows reverse the active list order.
- Faster filter access through an optional, bounded PSRAM index cache, reducing repeated microSD metadata reads. Home cover caches and file-list rendering also reuse work.
- Optional **daily reading goal**, a simple completion calendar and a **Reading goal sleep wallpaper** with progress and goal streak. The Today card shows the target; recorded daily time stays in the card above. Change an existing target from its calendar.
- A temporary return button after a table-of-contents jump; it disappears when you turn the page. Reader controls have improved spacing and two-line chapter names.
- Optional counterclockwise rotation of **XTC/XTCH covers**, without changing book-page orientation.

### Apps and games
- Consistent controls and spacing across PET, Reading Quest, Habits, Notes, Pomodoro, Flashcards, Words and the other tools.
- Larger, balanced Clock date/day labels; refinements to Pomodoro, study history and reading-statistics cards.
- Swipe navigation in more game guides; clearer Go and Checkers help and settings.
- Picross avoids unnecessary redraws on unrelated touches. 2048 has victory/defeat screens, and Carnivore returns to its menu when leaving a case.
- Flashcard review history uses five larger session rows per page and a bounded history window.

### Downloads and installation
**For Xteink X4 Pro only.** Back up your microSD before updating.

- `CrossNext-X4PRO-OTA.bin`: update through the reader's firmware updater. Keep this filename for online updates.
- `CrossNext-X4PRO-Firmware-v1.4.0.zip`: application and factory images, installation notes, licences and checksums. The factory image is for compatible USB installation/recovery, not the microSD updater.
- `CrossNext-Resources.zip`: extract and merge its folders into the microSD root. **PET, Reading Quest and The Intercept need their resource folders; firmware installation does not install them.** Dictionaries and optional fonts are included. Keep your books and `.crosspoint` data.
- `SHA256SUMS-v1.4.0.txt`: checksums for these downloads.

Existing Resources remain compatible. The package contains no personal saves. Physical device timing depends on the microSD, cover images and library size; simulator and automated tests do not replace device testing.

## Español

### Inicio e interfaz
- Dos temas nuevos: **Carrusel 2**, con portadas grandes y seis accesos, y **Estantería**, con una cuadrícula de libros, progreso de lectura y menú lateral.
- Elige por separado el tema de Inicio y el del resto de la interfaz, con opciones por sección. Marco y Carrusel conservan sus cajas rectas; Carrusel 2 y Estantería usan cabeceras claras y controles redondeados.
- Tipografía, márgenes, iconos y controles más coherentes en español e inglés, en vertical y horizontal. Noto Sans sigue siendo la predeterminada. Ubuntu y Noto Serif continúan disponibles; se elimina Jersey 25 y sus preferencias guardadas pasan a Noto Sans. La letra del libro sigue siendo independiente.
- Hasta siete accesos en Inicio. En Estantería, Apariencia permite elegir el número de páginas; en los demás temas se elige la cantidad de libros recientes.

### Biblioteca y lectura
- Selector **Recientes / Filtrar**: género, serie, nombre o apellido del autor y primera letra del título. Selecciona un valor para ver sus libros. Las series mantienen el orden de volumen cuando está en los metadatos.
- Hasta 30 libros abiertos en Recientes, nueve filas por página vertical, títulos de dos líneas y cambio entre lista y portadas. Las flechas invierten el orden.
- Filtros más ágiles mediante una caché opcional y limitada en PSRAM que evita lecturas pequeñas repetidas de la microSD. También se reutiliza trabajo al volver a Inicio y al dibujar listas.
- **Meta diaria de lectura**, calendario sencillo de cumplimiento y **fondo al suspender con la meta**, progreso y racha. La tarjeta de Hoy muestra la meta; el tiempo leído está en la tarjeta superior. Una meta ya configurada se modifica desde su calendario.
- Botón temporal para regresar tras saltar desde el índice; desaparece al cambiar de página. Controles del lector con mejores márgenes y capítulos de dos líneas.
- Opción de girar las **portadas XTC/XTCH** 90° en sentido antihorario, sin girar las páginas.

### Apps y juegos
- Controles y márgenes más coherentes en PET, Aventura lectora, Hábitos, Notas, Pomodoro, Flashcards, Palabras y otras herramientas.
- Día y fecha del Reloj más legibles y equilibrados, y mejoras en Pomodoro, historial de repaso y estadísticas.
- Más guías de juegos admiten deslizamiento; ayuda y ajustes de Go y Damas más claros.
- Picross evita repintados al tocar zonas ajenas al tablero. 2048 muestra victoria o derrota y Carnívoro vuelve a su menú al salir de un caso.
- Historial de Flashcards con cinco sesiones más legibles por página y una ventana de historial limitada.

### Archivos e instalación
**Solo para Xteink X4 Pro.** Guarda una copia de tu microSD antes de actualizar.

- `CrossNext-X4PRO-OTA.bin`: actualización desde el lector. Conserva el nombre para las actualizaciones en línea.
- `CrossNext-X4PRO-Firmware-v1.4.0.zip`: imágenes de aplicación y fábrica, instrucciones, licencias y sumas. La imagen de fábrica sirve para instalación o recuperación USB compatible, no para el actualizador de la microSD.
- `CrossNext-Resources.zip`: extrae y combina sus carpetas en la raíz de la microSD. **PET, Aventura lectora y The Intercept requieren sus recursos; instalar el firmware no los copia.** Incluye diccionarios y fuentes opcionales. Conserva tus libros y `.crosspoint`.
- `SHA256SUMS-v1.4.0.txt`: sumas de los archivos descargables.

Los Resources anteriores siguen siendo compatibles. El paquete no incluye partidas personales. Los tiempos reales dependen de la microSD, las imágenes y la biblioteca; las pruebas automáticas y el simulador no sustituyen la prueba física.
