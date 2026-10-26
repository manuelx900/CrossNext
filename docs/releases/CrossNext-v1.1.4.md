# CrossNext v1.1.4 — Statistics and book progress in one sync · Xteink X4 Pro

## English

- **One Sync button for statistics and book progress.** Use the circular-arrow shortcut in Reading Stats to synchronize your reading history and the positions of books on the microSD. You no longer need to open each book to sync its progress. The shortcut appears after configuring a compatible CrossPoint Sync account.
- **Keep the furthest position.** A local advance is uploaded; a later cloud position is restored on the reader. Read offline, leave the book to save, then synchronize when connected.
- **More than Recent Books.** The position pass includes EPUB, XTC/XTCH, TXT and Markdown in visible book folders, including books outside Recents and files not yet opened locally when matching cloud progress exists. Books that have never been started remain unread. Book files themselves are not downloaded.
- **Recoverable interruptions.** If positions cannot finish after statistics have synced, the screen reports partial completion. Completed changes are kept; retrying skips positions already equal and does not add the same local statistics twice.
- **Safer microSD error handling.** New XTC/XTCH save folders are created when needed, and directory or document-ID read failures are reported instead of being mistaken for successful synchronization.
- **Wi-Fi stays enabled when leaving its settings.** Returning from Wi-Fi or closing the control center no longer switches the radio off. The switch reflects its actual state. Sleep, restart and network apps retain their existing connection lifecycle; this does not add automatic connection on wake.

The individual book's Sync Progress command remains available for explicit local/remote choices. Bulk sync keeps the furthest mapped position. Use the same book edition on both devices.

## Español

- **Un solo botón para estadísticas y progreso.** Usa el atajo circular de Estadísticas para sincronizar el historial de lectura y las posiciones de los libros de la microSD. Ya no necesitas abrir cada libro para sincronizar su avance. El atajo aparece al configurar una cuenta compatible de CrossPoint Sync.
- **Se conserva la posición más adelantada.** Un avance local se sube a la cuenta; una posición posterior de la nube se recupera en el lector. Puedes leer sin conexión, salir del libro para guardar y sincronizar después.
- **Incluye libros fuera de Recientes.** Se sincronizan EPUB, XTC/XTCH, TXT y Markdown de las carpetas de libros visibles, incluidos archivos que todavía no hayas abierto en este lector cuando exista progreso coincidente en la nube. Los libros que nadie haya empezado siguen sin empezar. No se descargan los archivos de los libros.
- **Reintentos tras una interrupción.** Si las estadísticas terminan y quedan posiciones pendientes, aparece un aviso de finalización parcial. Los cambios completados se conservan; al reintentar se omiten las posiciones ya iguales y no se vuelven a sumar las mismas estadísticas locales.
- **Mejor gestión de errores de la microSD.** Las carpetas de guardado XTC/XTCH se crean cuando hacen falta. Los errores al leer directorios o identificadores se notifican, en lugar de confundirse con una sincronización completada.
- **Wi-Fi conserva su estado al salir de sus ajustes.** Volver de Wi-Fi o cerrar el centro de control ya no apaga la radio. El interruptor refleja su estado real. El reposo, el reinicio y las aplicaciones de red conservan su gestión de conexión; este cambio no añade conexión automática al despertar.

La sincronización individual de cada libro sigue disponible para elegir explícitamente entre la posición local y la remota. La sincronización conjunta conserva la posición más adelantada. Utiliza la misma edición del libro en ambos dispositivos.

## Install / Instalación

For **Xteink X4 Pro only**. Back up the microSD and follow the [installation guide](https://github.com/manuelx900/CrossNext/blob/xteink/docs/install.md). Compatible firmware updates preserve saves under `/.crosspoint/`.

Para **Xteink X4 Pro**. Guarda una copia de la microSD antes de actualizar. Usa `CrossNext-X4PRO-Firmware-v1.1.4.zip`; el archivo `CrossNext-X4PRO-OTA.bin` adjunto permite las actualizaciones desde el firmware cuando este Release esté publicado.

**PET, Reading Quest and The Intercept require their microSD contents.** Extract `CrossNext-Resources.zip` to the card root, retaining `/apps/pet/`, `/apps/reading-adventure/` and `/apps/intercept/`. The resource pack is unchanged; a complete existing installation can stay.

**PET, Aventura lectora y The Intercept necesitan sus contenidos en la microSD.** Extrae `CrossNext-Resources.zip` en la raíz de la tarjeta. Si ya tienes instalado el paquete completo, puedes conservarlo. Actualizar el firmware no instala esos contenidos.
