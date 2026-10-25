# PET — recursos para microSD

Extrae `apps/pet/` del ZIP de instalación en la raíz de la microSD.
Si trabajas desde el repositorio, ejecuta los seis comandos de reconstrucción
indicados abajo y copia `resources/apps/pet/` a `/apps/pet/` en la tarjeta.
Deben quedar:

- `/apps/pet/graphics.bin`: 64 ilustraciones: huevos, mascotas, objetos y la mano de acariciar.
- `/apps/pet/home.bin`: 39 poses de Hogar (despierta, dormida y enferma) y seis mitades de los fondos exclusivos de Bosque, Lunar y Volcán.
- `/apps/pet/habitat.bin`: habitación, 240 accesorios y máscaras para colocar las poses de Hogar.
- `/apps/pet/farewell.bin`: tres espíritus de despedida con máscaras.
- `/apps/pet/breathing.bin`: 216 fotogramas de respiración, nueve de huevo y doce cabezas para refugios, con sus máscaras.
- `/apps/pet/collection.bin`: copa y laureles de la colección completa; se carga en la ventana del trofeo y en las tarjetas de PET/Hogar completados después de eclosionar. Se reutiliza entre esas vistas y se libera al salir de ellas.

Reemplaza los seis archivos para usar la colección completa. No borres
`/.crosspoint/pet/`: contiene el nombre, progreso e historial de tu mascota.
Cierra PET antes de cambiar la tarjeta y vuelve a abrir la app para recargar el arte.

Las ilustraciones vienen de `pet-pixel-bw-v1.zip`, con 102 PNG de 1 bit y
256 × 256 píxeles. Los 15 alias de huevos y poses despiertas reutilizan sus
originales. No incluyen controles, fuentes, textos ni partidas:
Hogar compone la habitación y sus muebles según los objetos equipados.

El dispositivo lee los seis paquetes desde la microSD; los dibujos no están
incluidos en la imagen de firmware. Si faltan, están dañados o no son compatibles, conserva la
partida y muestra el aviso para instalar los contenidos. Los seis son necesarios.

Para reconstruir desde los PNG suministrados, en la raíz del repositorio:

    python tools_local/pet/build_resources.py
    python tools_local/pet/build_resources.py --home
    python tools_local/pet/build_habitat.py
    python tools_local/pet/build_breathing.py
    python tools_local/pet/build_farewell.py
    python tools_local/pet/build_collection.py

Los originales finales se conservan en `assets_local/pet/png/`, sin cambiar sus
bytes. Al empaquetar se retira el margen blanco sobrante, conservando ocho
píxeles de aire y todos los píxeles de tinta, sin escalar ni suavizar. Los
`source_bounds` del manifiesto registran el área utilizada. `source.json` registra la procedencia y SHA-256 de cada imagen.
`manifest.json` y `home-manifest.json` describen los ID estables y los paquetes
CNART002 generados. No cambies el orden de las ranuras.

El ZIP `PET-HOGAR-ILUSTRACIONES-v1.zip` aporta el fondo y 240 accesorios, sesenta
por categoría. Sus originales se conservan en `hogar/`. El constructor usa las
coordenadas reales de las láminas, recorta cada objeto y genera PNG de 1 bit,
máscaras y `habitat.bin` (560 registros). Las dimensiones y apoyos por mueble
están en `hogar/catalogo-integrado.json`; ese archivo conserva los ID antiguos
0–11. La habitación ocupa una escena de proporción 2:1, con anclajes separados
para suelo, pared y techo. Las máscaras impiden que el suelo atraviese al pet.
El pet crece visualmente por edad y su pose de descanso se apoya en el mueble.
`habitat-manifest.json` contiene los registros, dimensiones y checksum del pack.

Los atlas de respiración se conservan en `animation/`, con sus prompts y
procedencia. Las tres mascotas y cuatro edades tienen tres fases de respiración
para las poses normal, alegre, hambrienta, cansada, enferma y dormida: 72 poses,
216 fotogramas, nueve de tambaleo de huevo y doce cabezas estáticas con máscaras (474 registros). La
respiración funciona en PET, Hogar, Recompensa y Evolución; las Z aparecen
al dormir sana en PET, Hogar y Recompensa y llevan contorno blanco. Los huevos se tambalean en tres
ángulos. Un indicador monocromo de ánimo alterna dos tamaños sobre el pet. Las
vidas finalizadas permanecen estáticas. Los respaldos internos de dibujo no
sustituyen la instalación del paquete completo. Reemplaza los seis paquetes
junto con el firmware nuevo; no se modifican el nombre, inventario o progreso.
En camas y hamacas se ve el cuerpo a una escala acorde con el mueble; en las
casitas y cuevas asoma solo la cabeza, con el encuadre definido por cada abertura.

Las despedidas originales y sus prompts se conservan en `farewell/`. Silva,
Nox y Ember tienen ojos cerrados, aureola y un cuerpo de espíritu. El conversor
genera imágenes de 1 bit de hasta 208 × 240 y pares máscara/tinta (seis registros).
`farewell-manifest.json` registra dimensiones, procedencia y SHA-256. El paquete
se abre al entrar en una despedida; su presencia y compatibilidad se comprueban
al abrir PET junto con los demás paquetes.
El aviso de fallecimiento se guarda al pulsar Continuar o Ver recuerdos. Las
generaciones aparecen como Gen 1, Gen 2, Gen 3… y conservan las reglas actuales.

La mano suministrada se conserva en `ui/petting-hand.png` y ocupa el ID 63 de
`graphics.bin`. Se convierte a blanco y negro y se ajusta a 144 × 100 como máximo
al construir el paquete, sin alterar los PNG originales de las mascotas. Los
ID 0–62 permanecen estables. El simulador también acepta el paquete anterior de
63 ilustraciones; en ese caso utiliza la mano de respaldo.

Los fondos suministrados se conservan sin alterar en `backgrounds/`, con su
procedencia y SHA-256 en `backgrounds/source.json`. Bosque usa Silva, Lunar usa
Nox y Volcán usa Ember. Se ajustan proporcionalmente a 448 píxeles de ancho,
se recortan o rellenan por arriba hasta 144 de alto y se convierten a un bit,
sin suavizado ni recorte de tinta. Las dos mitades de 224 × 144 ocupan los ID
39–44 de `home.bin`; las 39 poses originales conservan sus ID. Solo PET dibuja
estos paisajes, detrás del cuerpo y el indicador de ánimo. Hogar y Recuerdos
conservan sus escenas. El firmware acepta también el paquete anterior de 39
registros, con el fondo blanco y las poses habituales.

Los PNG `ui/reference-*.png` son las referencias originales suministradas para
Salud, resultados, evolución y estados vacíos. `ui/reference-icons.json` conserva
los recortes, tamaños y SHA-256. Ejecuta
`python tools_local/pet/build_reference_icons.py` para regenerar
`src/apps_local/pet/PetReferenceIcons.h`: veinte imágenes monocromas, 45.088 bytes
de bitmap constantes en flash. El remedio disponible omite el visto bueno;
solo la variante sin tratamiento pendiente lo incluye. Las ilustraciones grandes
de celebración de Silva y limpieza de Ember se reservan a adultos sanos y despiertos;
las demás especies, edades y estados usan sus ilustraciones existentes.

`ui/reference-empty-home.png` aporta los cuatro iconos de categoría vacía y el
recuadro discontinuo con el signo más del proyecto sin selección. Sus cinco
recortes añaden 2.560 bytes y se regeneran con el mismo conversor.

Los ajustes `clear` y `copy` conservan las fuentes originales: eliminan el resto
del distintivo junto a la tapa del remedio y sustituyen el corazón izquierdo de
Silva por una copia íntegra del corazón del bocadillo derecho.

`ui/reference-album.png` y `ui/reference-album-actions.png` aportan la pila de
libros y el icono del botón Ver recuerdos: dos recortes, 5.280 bytes adicionales.
