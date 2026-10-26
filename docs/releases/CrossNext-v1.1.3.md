# CrossNext v1.1.3 — Unified reading statistics and Home improvements · Xteink X4 Pro

## English

- **CrossPoint Sync statistics.** Recover your account history, read offline and upload new reading on the next manual sync. Reading Stats shows one combined history without adding the same local sessions twice. The circular-arrow shortcut beside the battery appears only after configuring a compatible account. Existing reader position sync stays available separately.

- **File Transfer on Home by default.** The Framed menu starts with Browse Files, Recent Books, File Transfer, Reading Stats, Games, Apps and Settings. You can enable up to eight entries. Existing customized menus keep their choices.
- **More balanced Home layouts.** Framed uses slightly smaller labels with eight entries and closes the last visible row with a bottom divider. Carousel centers the remaining shortcuts when entries are hidden.
- **Fast page turns count.** Reading statistics no longer require two seconds per page or ten seconds per session. Short split-page manga in XTC/XTCH counts successful forward turns too. Subsecond reading time accumulates across pages and rounds once when the session is saved.
- **XTC/XTCH metadata.** Title and author are read from the position declared in the file, while retaining support for the older fixed-position layout.
- **XTCH in Library.** XTC and XTCH now appear with their embedded titles and authors when Library metadata is enabled. This adapts [CrossPoint #4023](https://github.com/crosspoint-reader/crosspoint-reader/pull/4023) by Eszter Schuffert. Refresh the Library index after updating to include existing files.
- The Spanish game title is now **Carnívoro**.

Enable **Settings > Reading > Reading Formats > XTC / XTCH** to include these formats in statistics. Sessions save when leaving a book or preparing for sleep. A synchronized device clock is still required; time displays in whole minutes may show zero for a shorter saved session.

## Español

- **Estadísticas con CrossPoint Sync.** Recupera el historial de tu cuenta, lee sin conexión y sube las nuevas lecturas en la siguiente sincronización manual. Estadísticas muestra un historial combinado sin sumar dos veces las mismas sesiones locales. El atajo circular junto a la batería solo aparece al configurar una cuenta compatible. La sincronización de la posición del libro sigue disponible por separado.

- **Enviar archivos en Inicio por defecto.** Framed muestra Archivos, Biblioteca, Enviar archivos, Estadísticas, Juegos, Apps y Ajustes. Puedes activar hasta ocho opciones. Los menús ya personalizados conservan su selección.
- **Inicio mejor equilibrado.** Framed reduce ligeramente la letra con ocho opciones y añade la línea inferior al último selector visible. Carrusel centra los accesos restantes al ocultar opciones.
- **Los cambios rápidos de página cuentan.** Las estadísticas ya no exigen dos segundos por página ni diez segundos por sesión. También cuentan los avances válidos de manga con páginas divididas en XTC/XTCH. Las fracciones de segundo se acumulan y se redondea una sola vez al guardar la sesión.
- **Metadatos XTC/XTCH.** El título y el autor se leen desde la posición indicada por el archivo, manteniendo la compatibilidad con la ubicación fija antigua.
- **XTCH en Biblioteca.** XTC y XTCH muestran el título y el autor incluidos en el archivo cuando los metadatos de Biblioteca están activados. Se adapta la corrección de CrossPoint citada arriba. Actualiza el índice de Biblioteca tras instalar el firmware para incluir los archivos existentes.
- El juego aparece como **Carnívoro** en español.

Activa **Ajustes > Lectura > Formatos de lectura > XTC / XTCH** para incluir estos formatos en las estadísticas. Las sesiones se guardan al salir del libro o al suspender. El reloj debe estar sincronizado; los indicadores en minutos pueden mostrar cero si la sesión guardada dura menos de un minuto.

## Install / Instalación

For **Xteink X4 Pro only**. Follow the [installation guide](https://github.com/manuelx900/CrossNext/blob/xteink/docs/install.md). Back up the microSD before updating. Firmware updates preserve saved data under `/.crosspoint/`.

**PET, Reading Quest and The Intercept require their microSD contents.** Extract `CrossNext-Resources.zip` to the card root so `/apps/pet/`, `/apps/reading-adventure/` and `/apps/intercept/` are directly on the card. The resource pack is unchanged; an already installed complete pack can stay. Updating the firmware does not install these contents.

**PET, Aventura lectora y The Intercept necesitan sus contenidos en la microSD.** Extrae `CrossNext-Resources.zip` en la raíz de la tarjeta, conservando las carpetas indicadas arriba. Si ya tienes el paquete completo, puedes conservarlo. Actualizar el firmware no instala estos contenidos.
