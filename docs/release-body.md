# CrossNext v1.1.5 — Reader controls and app polish · Xteink X4 Pro

## English

- **Mark books as finished.** Open a book, swipe up from the bottom edge, choose **More → Finish book** and confirm. EPUB and XTC/XTCH progress becomes 100% and the book appears as finished in Reading Stats, without adding reading time or pages you did not read.
- **Swipe through More.** Move horizontally between the reader menu's pages. Swiping at either end keeps the menu open and does not turn the book underneath.
- **Consistent square controls.** Menus, buttons and reader sheets keep square corners, including installations with an older saved theme.
- **A larger optional header clock.** The clock now also appears in Solitaire and Hearts. It respects the 12/24-hour format, battery visibility and game controls, and is omitted where there is insufficient room. Outside Home, it updates when the screen repaints.
- **Better Home-button input.** Improved double-tap handling after Home has been idle.
- **Clearer settings.** Reading Formats is now **Statistics tracking**, and unsupported legacy controls are omitted from device and web settings.
- **Aligned Flashcards instructions.** The Question and Answer examples and their connecting arrow are centered consistently in English and Spanish.

## Español

- **Marcar libros como terminados.** Abre un libro, desliza hacia arriba desde el borde inferior, entra en **Más → Terminar libro** y confirma. El progreso EPUB y XTC/XTCH queda al 100% y el libro figura como terminado en Estadísticas, sin sumar tiempo ni páginas que no hayas leído.
- **Deslizar entre páginas de Más.** Cambia de página del menú horizontalmente. Al llegar a un extremo, el menú permanece abierto y el libro no cambia de página.
- **Controles con esquinas rectas.** Menús, botones y desplegables conservan las esquinas rectas, incluso si había un tema antiguo guardado.
- **Reloj de cabecera más grande.** También aparece en Solitario y Corazones. Respeta el formato de 12/24 horas, la visibilidad de la batería y los controles del juego, y se oculta donde no hay espacio suficiente. Fuera de Inicio, se actualiza al repintar la pantalla.
- **Mejor respuesta del botón Home.** Mejorada la detección del doble toque tras dejar Inicio inactivo.
- **Ajustes más claros.** Formatos de lectura pasa a llamarse **Registro de estadísticas**. Las opciones heredadas no compatibles se ocultan tanto en el dispositivo como en la configuración web.
- **Guía de Flashcards alineada.** Los ejemplos de Pregunta y Respuesta y la flecha entre ambos quedan centrados en español e inglés.

## Install / Instalación

**Xteink X4 Pro only.** Back up the microSD before updating. Download `CrossNext-X4PRO-Firmware-v1.1.5.zip` and follow the [installation guide](https://github.com/manuelx900/CrossNext/blob/xteink/docs/install.md). The ZIP contains application and factory images; the factory image is for full installation/recovery. `CrossNext-X4PRO-OTA.bin` is the application-only asset used by the built-in updater.

**Solo Xteink X4 Pro.** Guarda una copia de la microSD antes de actualizar. El ZIP incluye las imágenes de aplicación e instalación completa; usa la imagen de fábrica para instalación o recuperación. El archivo `CrossNext-X4PRO-OTA.bin` permite la actualización desde el firmware cuando este release esté publicado.

**PET, Reading Quest and The Intercept require the microSD contents.** Extract `CrossNext-Resources.zip` to the card root, keeping `/apps/pet/`, `/apps/reading-adventure/` and `/apps/intercept/`. The resource pack is unchanged from v1.1.4; a complete existing installation remains compatible.

**PET, Aventura lectora y The Intercept necesitan sus contenidos en la microSD.** Extrae `CrossNext-Resources.zip` en la raíz de la tarjeta. Si ya tienes el paquete completo de v1.1.4, puedes conservarlo: actualizar el firmware no instala esos contenidos.

Manual completion records use a new statistics event. Back up `/.crosspoint/reading-history/` before downgrading: older firmware ignores manual completions and can replace their book state if further reading is recorded. Avoid recording or syncing statistics with older versions.

Las finalizaciones manuales usan un nuevo evento de estadísticas. Guarda una copia de `/.crosspoint/reading-history/` antes de volver a una versión anterior: el firmware antiguo ignora estas finalizaciones y puede reemplazar el estado del libro si se registra más lectura. Evita registrar o sincronizar estadísticas con versiones anteriores.
