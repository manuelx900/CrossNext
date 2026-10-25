# PET: respiración de todas las poses

Los 19 atlas se editaron con la herramienta integrada `image_gen`, usando las
poses existentes como objetivos. Tres atlas cubren descanso y quince cubren
normal, alegre, hambrienta, cansada y enferma. El atlas restante contiene tres
huevos por tres ángulos. Los atlas de mascotas contienen las cuatro
edades por tres fases. Los prompts exactos y SHA-256 acompañan al arte; los PNG
originales y los 240 accesorios de Hogar se conservan.

`tools_local/pet/build_breathing.py` detecta los espacios reales entre filas y
columnas, registra las siluetas por su borde izquierdo y apoyo inferior y aplica
una escala común a las fases de cada pose. La conversión a 1 bit y las máscaras
se preparan fuera del dispositivo. El lector no genera ni deforma imágenes.

`/apps/pet/breathing.bin` tiene 474 registros CNART002:

- 0–71: doce poses dormidas × tres fases × máscara/tinta.
- 72–95: doce cabezas estáticas × máscara/tinta para refugios cerrados.
- 96–455: especie (Silva, Nox, Ember), edad (recién nacida, cría, juvenil,
  adulta), pose (normal, alegre, hambrienta, cansada, enferma), fase (0, 1, 2),
  máscara/tinta. Son 60 poses × tres fases × dos registros.

- 456–473: nueve inclinaciones de huevo (Silva, Nox, Ember × izquierda, centro,
  derecha) con máscara/tinta. Se alinean por el centro y la base de apoyo.

Las 72 poses aportan 216 fotogramas, además de los nueve fotogramas de huevo. `breathing-manifest.json` documenta cada ID,
procedencia, tamaño y CRC. Se conservan los primeros 96 ID; reemplaza firmware y
paquete juntos, porque el firmware nuevo requiere 474 registros. Un paquete
anterior o dañado activa el respaldo estático.

El ciclo 0→1→2→1 avanza cada 1,5 segundos mientras el pet activo
está visible en PET, Hogar, Recompensa o Evolución. Enfermas y cansadas conservan
su expresión; PET y Hogar comparten la selección de pose, incluida la enferma y
la de descanso. PET siempre muestra el cuerpo completo; sólo Hogar puede
mostrar la cabeza dentro de un refugio. El huevo se tambalea en tres ángulos. Los menús,
miniaturas históricas y vidas finalizadas permanecen estáticos. La lectura no
activa ningún refresco de PET. Sin arte opcional utilizable tampoco se refrescan
las poses estáticas; las Z de descanso de Hogar y los indicadores de ánimo siguen funcionando por
código. Los indicadores monocromos alternan dos tamaños centrados; distinguen
alegría, tristeza, hambre, cansancio y enfermedad sin usar caracteres emoji del
sistema. Las Z llevan un halo blanco bajo cada trazo negro.

No hay tareas adicionales ni cambios de formato de partida. Se reutiliza el
lector HAL y su caché PSRAM (474 × 16 + 4096 = 11680 bytes), liberado al salir,
con lecturas pequeñas de respaldo si no se puede asignar. No se añade un
framebuffer ni asignaciones por fotograma. Sigue usándose el refresco completo
actual; velocidad, parpadeo, consumo y rastros requieren prueba física.

`assets_local/pet/hogar/rest-poses.json` identifica 23 muebles con abertura. En
ellos asoma la cabeza y el torso que respira queda oculto. Las camas y hamacas
muestran el cuerpo completo a una escala acorde con su apoyo. Las Z se colocan
por encima del mueble o a un lado si hay un adorno colgante.
