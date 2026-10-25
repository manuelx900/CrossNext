# CrossNext v1.0.0 — Xteink X4 Pro

CrossNext reúne lectura, estudio, herramientas personales y juegos en el Xteink
X4 Pro. Es un derivado de **CrossPlay 1.14.2**, basado en **CrossPoint 1.6.5**,
con referencias de interfaz de CrossInk y VCodex. Conserva las licencias y la
autoría del lector, el SDK y los componentes heredados.

## Novedades de CrossNext

- Inicio con diseños Marco y Carrusel, navegación física centrada en las opciones
  del menú y conservación de pulsaciones rápidas durante el refresco. Los accesos
  a juegos y apps muestran su propio nombre e icono.
- Tipografía de Inicio con títulos y autores diferenciados; progreso de Marco
  en un rectángulo compacto. El porcentaje se reutiliza al volver a Inicio cuando
  el progreso guardado no ha cambiado.
- Biblioteca con selección visible y trabajo de portadas separado de la navegación.
- Estadísticas con cinco libros en vertical y tres en horizontal; minutos de varios
  dígitos, con la unidad debajo. Las pestañas reutilizan datos de la carga inicial.
- Flashcards de Anki con paginación completa, tamaños independientes de pregunta
  y respuesta entre 10 y 18 pt, y ajuste inteligente activado de fábrica.
- Imagen ampliada de Anki con detalle original cargado solo al usar Zoom, giro,
  arrastre y flechas que recorren la imagen por tercios y giran con ella hacia
  la izquierda o la derecha. Foto ocupa un control
  compacto junto al selector de páginas.
- Palabras desde el lector/diccionario, tipografía legible y contenido organizado
  por etiquetas. La importación de Anki queda en Flashcards; Palabras ofrece
  Configuración en su barra inferior.
- Gestos horizontales de paginación en las apps y selectores compatibles; Pomodoro
  permite iniciar y pausar con el botón físico.
- Asignación de pulsación, doble pulsación y pulsación mantenida en pantallas
  completas, con una app o juego concreto independiente para cada gesto.
- Control rápido de luz y calidez con iconos de estado y recuperación del valor
  anterior; giro hacia la izquierda por defecto y dirección configurable.
- Fondos con preparación visible, progreso, caché de miniaturas y selección de
  varias imágenes entre páginas para el fondo aleatorio de suspensión. «Todos»
  marca o desmarca las imágenes válidas; «Listo» pide confirmación antes de guardar.
  Las imágenes dañadas quedan fuera de la selección.
- Logo monocromo actualizado en arranque y suspensión; simulador poblado con
  ejemplos para mostrar el proyecto.

## Archivos adjuntos

| Archivo | Contenido |
| --- | --- |
| `CrossNext-X4PRO-Firmware-v1.0.0.zip` | Aplicación y binario de instalación completa, instrucciones y SHA-256. |
| `CrossNext-Resources.zip` | Recursos de microSD: fuentes, diccionarios y gráficos, con sus avisos. |
| `CrossNext-source.zip` | Fuentes limpias, pruebas, documentación y las imágenes del README. |

La aplicación `.bin` se usa con una actualización compatible por SD/OTA; el
`-factory.bin` incluye arranque y particiones para instalación USB en `0x0`.
Lee [la guía de instalación](../install.md). Solo X4 Pro está incluido en esta
entrega. Una instalación sin configuración previa comienza en inglés y permite
cambiar a español en Configuración > Idioma. Una actualización conserva el idioma.

La comprobación de actualizaciones consulta únicamente GitHub Releases de
`manuelx900/CrossNext`. Cada Release debe adjuntar `CrossNext-X4PRO-OTA.bin`,
idéntico a su aplicación versionada; no se instala el archivo factory por OTA.
Wallpapers usa imágenes locales de la SD; se retiraron la descarga de fondos,
el envío por QR desde móvil y Live heredados de CrossPlay.

## Créditos y comprobación antes de publicar

[CrossPlay](https://github.com/ma-r-s/crossplay),
[CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader),
[FreeInk](https://freeink.org/), [CrossInk](https://github.com/uxjulia/CrossInk) y
[VCodex](https://github.com/franssjz/cpr-vcodex). El
[detalle de procedencia](../crossnext-changes.md) distingue código heredado de
referencias de diseño. Conserva LICENSE y THIRD-PARTY-NOTICES.md.

Esta primera versión se ofrece para pruebas en X4 Pro y conserva errores
conocidos pendientes de corregir en versiones posteriores. Las comprobaciones
del simulador no miden el tiempo de refresco del panel real.

Los títulos de Inicio aprovechan también el espacio del autor cuando no existe.
Lectura y Controles muestran el contenido que continúa al final de la pantalla.
La selección inicial de diccionarios solo completa los espacios sin asignar;
conserva todos los nombres guardados previamente, incluso con marcas automáticas
procedentes de versiones anteriores.

Los contornos de las secciones mantienen su grosor también en las filas
parcialmente visibles, conservando la distribución, el espaciado y los tamaños
de letra de Lectura y Controles.
