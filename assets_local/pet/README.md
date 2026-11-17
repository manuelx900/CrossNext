# PET — recursos para microSD

Cierra PET y reemplaza la carpeta completa `/apps/pet-v2/` de la microSD con
`resources/apps/pet-v2/` o con `apps/pet-v2/` del ZIP de instalación. No borres
`/.crosspoint/pet/` ni `/.crosspoint/pet-v2/`: contiene la partida, nombre, inventario e historial.
Vuelve a abrir PET para recargar las ilustraciones. Los seis paquetes son
necesarios; si falta alguno o falla su checksum, PET conserva la partida y
muestra las instrucciones para instalar los contenidos.

## Colección actual

Los maestros están en `REDISEÑOS/PET/assets/v2/`. Conservan Silva, Nox y Ember,
sus cuatro edades nacidas y los estados existentes. El estilo usa ilustraciones
suaves con contorno negro y rellenos grises limpios, sin pixel art ni punteado.
Los retoques de Silva corrigen el punto sobre la sonrisa y la expresión adulta
cansada. La habitación conserva superficies claras, juntas visibles y un zócalo
fino. `provenance.json` registra prompts, herramienta y SHA-256 de los maestros.
Las poses de hambre usan un atlas específico en `REDISEÑOS/PET/assets/v5/`:
la mascota mira un cuenco vacío, con patas en posición natural y sin gestos de
tristeza ni lengua exagerada. Se mantienen los IDs, tamaños y demás estados.

- `graphics.bin`: 64 registros; huevos, mascotas, objetos iniciales y mano.
- `home.bin`: 52 registros. Poses 0–38, seis mitades de panoramas compatibles
  39–44, cuatro teselas verticales 45–48 y tres horizontales 49–51. Las teselas
  conservan la resolución de pantalla y evitan ampliar una miniatura.
- `habitat.bin`: 560 registros; fondo compatible, 240 muebles y máscaras/tinta
  para las poses de Hogar. Se conservan las dimensiones y apoyos existentes.
- `breathing.bin`: 1278 registros; 216 fotogramas de cuerpo, doce cabezas y
  nueve fotogramas de huevo, todos con sus máscaras. Los cuerpos nacidos
  permanecen estáticos fuera de las reacciones breves de curiosidad, cuidados, juego y comida; el reloj actual anima huevos e indicadores de ánimo.
  Los 60 registros nuevos añaden tres alimentos enteros/mordidos y dos poses
  comiendo para cada especie y edad, con sus máscaras. Los maestros y prompts
  están en `REDISEÑOS/PET/assets/v3/feeding/`. Los paquetes de 474, 534 y 582
  registros son históricos y no son compatibles con la app actual. Otros 48 registros
  incorporan seis alimentos (bayas, bizcocho, fruta nocturna, hamburguesa, pizza
  y sándwich) enteros, con dos porciones sucesivamente menores y plato vacío.
  Cada comida completa cuesta una provisión, aunque tenga tres mordidas.
  El atlas y prompt nuevos están en `REDISEÑOS/PET/assets/v4/`.
  Las últimas 48 ranuras añaden los dos fotogramas de curiosidad por especie
  y edad, con máscaras, desde `REDISEÑOS/PET/assets/v7/curiosidad/`.
- `farewell.bin`: seis registros; tres espíritus con máscaras.
- `collection.bin`: un registro de 280 × 210, 14.740 bytes; trofeo y laureles.

Los paquetes actuales usan CNART003: cobertura de tinta de dos bits y cuatro
tonos, índices estables y checksums por imagen. Los manifiestos describen las
dimensiones y SHA-256 de cada paquete. PET exige CNART003 y los tamaños actuales
indicados arriba; los formatos monocromos y Home de 45 o 39 registros muestran
el aviso de instalación. El decodificador genérico los conserva para otros usos.
Las ilustraciones se leen de la tarjeta; no se incluyen en el binario de firmware.

## Reconstrucción

Desde la raíz del repositorio. Si los maestros están fuera del worktree,
configura `CROSSNEXT_PET_MASTERS` con la ruta absoluta a `assets/v2/`, conservando
sus carpetas hermanas v3, v4 y siguientes. Los comandos requieren su archivo de procedencia
y fallan si faltan; nunca exportan el arte antiguo silenciosamente. `--legacy`
es una opción explícita de compatibilidad para pruebas, no para distribuir.

Desde PowerShell, por ejemplo:

    $env:CROSSNEXT_PET_MASTERS = 'C:/ruta/REDISEÑOS/PET/assets/v2'

Después:

    python tools_local/pet/build_resources.py
    python tools_local/pet/build_resources.py --home
    python tools_local/pet/build_habitat.py
    python tools_local/pet/build_breathing.py
    python tools_local/pet/build_farewell.py
    python tools_local/pet/build_collection.py

Los seis comandos y el empaquetador general seleccionan los maestros actuales
mediante `tools_local/pet/gray_assets.py`. No cambies el orden de las ranuras ni
reemplaces los archivos de partidas al instalar el arte. En pantallas compatibles,
PET utiliza cuatro grises reales; otras pantallas mantienen su render monocromo.

## Fuentes anteriores y compatibilidad

Los originales de `png/`, `hogar/`, `animation/`, `farewell/`, `collection/`,
`backgrounds/` y `ui/` se conservan sin modificar. Sus ZIP, manifiestos y prompts
sirven como procedencia y referencia de dimensiones. Los conversores anteriores
seguían un flujo de PNG de un bit/CNART002, recorte de margen blanco y fondos de
224 × 144 por mitad. Ese es el formato anterior, no la exportación predeterminada
actual. Las funciones de los conversores siguen disponibles para los fixtures
de compatibilidad; los comandos anteriores generan la colección nueva.

`hogar/catalogo-integrado.json` conserva los 240 ID y las medidas de colocación.
Las versiones nuevas se ajustan a esas mismas dimensiones. `ui/reference-*.png`
y `ui/reference-icons.json` conservan las referencias de iconos secundarios; su
constructor es `tools_local/pet/build_reference_icons.py`. Los controles principales
de PET usan los símbolos vectoriales del firmware.

`PetHabitat.h` places objects by depth in the reference room. Wall ornaments sit
on the left, above the bed; two mounted ornaments use separate lanes and scale
proportionally to fit. Toys and floor environment scenery render in front of the
furniture. Memories use the same anchors with the captured objects and preserve
the room background crop. Placement adjustments do not change object IDs, saved
profiles or image packs.

La reacción de curiosidad utiliza dos atlas nuevos en
`REDISEÑOS/PET/assets/v7/curiosidad/`: mirada con cabeza inclinada y parpadeo.
El empaquetador añade sus máscaras y dibujos en las ranuras 582–629 de
`breathing.bin`; las 582 anteriores conservan sus ID. Las versiones previas
del firmware admitían paquetes pequeños sin esta reacción; la app actual exige
el paquete completo de 1278 registros. Se reutiliza la caché de imágenes de PET.

Las reacciones de acariciar, limpiar, curar y jugar tienen dos ilustraciones
nuevas por especie y edad en `REDISEÑOS/PET/assets/v8/acciones/`. Los ocho atlas
incluyen maestros transparentes, prompts y procedencia. Sus 192 registros se
añaden en las ranuras 630–821; los anteriores conservan ID y dimensiones. Los
paquetes de 630, 582, 534 y 474 registros solo se admitían en firmware anterior.

Las ranuras 822–1013 añaden las mismas reacciones con escala corporal y apoyo
registrados; las 822 anteriores se conservan. La espuma, gotas y trazos de
movimiento ya no reducen el personaje al ajustar la imagen. El paquete de
822 registros conservaba la colocación anterior en versiones previas del firmware. Para actualizar
maestros, ejecutar primero `python tools_local/pet/gray_assets.py --update-registration`
y después regenerar `breathing.bin`. El encabezado
generado se valida durante el empaquetado y debe estar actualizado antes de
compilar firmware. Copiar el paquete nuevo a la SD para activar la corrección.


The new pose and food masters, prompts and provenance are in
`REDISEÑOS/PET/assets/v10/personality/`. The personality version of breathing.bin has 1182 records:
0-1013 are unchanged; pet foods use 1014-1037 and registered poses 1038-1181.
Run gray_assets.py --update-registration, then build_breathing.py, and copy the
new pack to SD. Expressions extend PetMoodMarks.h; the rejected emotions.png
is excluded. PET8 remembers new food IDs 6-8 with unchanged cost/effect and
fixed save layout. Existing food IDs continue writing PET7.


Rest transitions add two walking frames in both directions for all species and
hatched stages. The current breathing.bin has 1278 records, preserving 0-1181
and adding walk mask/ink pairs in 1182-1277. Masters and prompts are under
`REDISEÑOS/PET/assets/v11/rest/`. Run `gray_assets.py --update-registration`
then `build_breathing.py`, and update the firmware and SD pack together. Earlier
packs are historical inputs; PET requires all 1278 records in the current grayscale format.
