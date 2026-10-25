# Corpus local del simulador

Descargado el 19 de octubre de 2026 para probar CrossNext. Estos archivos son
datos de la SD del simulador, no forman parte del firmware de dispositivo.

Hay **31 mazos distintos**, 6.603 notas y 9.203 tarjetas en los paquetes de
origen. `manifest.json` conserva la procedencia, SHA-256, tamaño, número de notas,
tarjetas y medios de cada paquete, además de su ruta dentro de `/FLASHCARDS`.

- 2 mazos médicos de [AnkiWeb](https://ankiweb.net/shared/decks?search=medical).
  AnkiWeb pidió iniciar sesión después de esas descargas; no se eludió ese límite.
- 6 mazos de [AK1089](https://github.com/AK1089/anki-decks): alfabeto, IPA, países,
  tabla periódica, metro de Londres y metro de Ámsterdam.
- 7 de [tltaylor1](https://github.com/tltaylor1/anki-decks): programación, cloud y seguridad.
- 5 de [AnkiWordBank](https://github.com/Ankiwordbank/free-anki-decks): español,
  francés, alemán y portugués. Licencia CC BY-NC 4.0; atribución a AnkiWordBank.
- 8 de [MilesCranmer](https://github.com/MilesCranmer/anki_science): informática,
  Unix, estadística, astronomía y física.
- 2 de [taivop](https://github.com/taivop/anki-decks): aprendizaje y lectura.

Se conservan los APKG originales, incluidos sus medios y plantillas. La importación
del firmware decide qué tarjetas puede convertir; el total de origen **no promete
9.203 tarjetas utilizables**. Hay tarjetas básicas, inversas, cloze, oclusión de
imágenes, formatos personalizados y audio para observar los límites reales.
No se ejecutan scripts de los repositorios descargados.

## Organización

La raíz mezcla archivos sueltos con `Idiomas`, `Medicina`, `Viajes`, `Lecturas`,
`Ciencias` y `Tecnologia`. `Ciencias` contiene `Fisica` y `Matematicas`;
`Tecnologia` contiene `Seguridad`. El firmware muestra tres entradas por página.
Cada APKG se prepara al abrirlo; las carpetas permiten subir con Atrás.

## Diccionarios

| Nombre en ajustes | Uso | Entradas |
| --- | --- | ---: |
| English Simple | Definiciones en inglés | 29.642 |
| Espanol Wikcionario | Definiciones en español | 123.686 |
| WikDict EN-ES | Inglés a español | 67.225 |
| WikDict ES-EN | Español a inglés | 21.952 |

Los monolingües son del [proyecto Wiktionary StarDict](https://xxyzz.github.io/wiktionary_stardict/),
edición 20261018, contenido Wiktionary CC BY-SA 4.0. Los bilingües son de
[WikDict](https://www.wikdict.com/page/download), contenido CC BY-SA 4.0.
Los archivos `.ifo` originales incluyen autor, fuente y atribución. Se mantienen
los índices, sinónimos y contenido originales, sin reducirlos a una lista de prueba.

Por defecto, **Definir** usa English Simple y **Traducir** usa WikDict EN-ES,
adecuados para Alicia en inglés. Para leer español se cambia el diccionario y,
si corresponde, el traductor en Ajustes → Lectura. Son selecciones independientes;
no hay traducción en línea ni detección automática del idioma del libro.

## Reinstalar

Con las dependencias locales del proyecto:

```powershell
.venv/Scripts/python.exe tools_local/wasm/seed_real_learning.py
```

El comando verifica todos los SHA-256 y sólo escribe en
`tools_local/wasm/sdcard`. `seed_learning.py` lo llama al regenerar las muestras.
Después hay que reconstruir los recursos web para actualizar la SD empaquetada.
Las muestras diminutas anteriores se guardaron de forma recuperable en
`qa-artifacts/real-learning-corpus/previous-samples`.

Los mazos de terceros se descargaron para esta prueba local. Antes de redistribuir
el corpus, se deben revisar las licencias de cada fuente; publicar el corpus no
forma parte de esta tarea.

## Anatomy and image preparation

MEDS2005 Anatomy adds 282 real cards and 367 media files, from
https://github.com/luke-a-thompson/AnkiDecks. Its original APKG is retained.
`Anatomia-preparada.apkg` and `Fisica-nuclear-preparada.apkg` flatten Image
Occlusion Enhanced masks into separate PNG question/answer faces, using
`tools_local/study/prepare_occlusion.py` (Pillow + resvg-py, computer only).
The manifest records original and derived hashes. No source notes are invented.
The simulator seeds those prepared files at the anatomy/physics paths and
exercises native APKG media import. The other original 29 decks remain.
