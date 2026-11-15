# Revisión contextual de traducciones de la UI

Fecha: 15 de noviembre de 2026. Esta revisión parte del árbol unificado, incluidos
los cambios anteriores de apps, juegos, lector e interfaz web.

## Resultado y alcance

Se corrigieron **1.068 valores adicionales, correspondientes a 90 claves en 36
idiomas**. De ellos, **911 valores pertenecen a claves que usa actualmente el
código**; los otros 157 corresponden a textos conservados en los catálogos.
No se cuentan de nuevo las 1.204 correcciones de la revisión anterior.

Se comprobaron estructuralmente los 39 catálogos: 38 idiomas naturales completos,
con 8.355 claves de referencia cada uno, y Ook, que usa intencionadamente textos
de respaldo en inglés. El programa actual referencia 7.134 claves.
Inglés y español conservan sus textos, incluido `Male` / `Female` en inglés.
Las formas nativas válidas, como el francés `Féminin`, no se consideran errores
por diferir del inglés.

La comprobación estructural cubre todas las claves de la UI, apps, juegos y web.
La revisión semántica se concentra en las familias documentadas y registra cada
corrección. **Esto no certifica la precisión nativa de cada frase de todos los
idiomas.** Los relatos largos de los juegos y las frases no revisadas necesitan
todavía revisión editorial por hablantes de cada idioma.

## Errores corregidos

| Contexto real | Interpretación incorrecta encontrada | Corrección |
| --- | --- | --- |
| Reloj: modo automático del horario de verano | Automóvil en chino y portugués | Modo automático |
| Guardar una selección, partida o configuración | Ahorrar dinero o rescatar algo | Escribir y conservar datos |
| PET: finalizar una vida y conservarla en el álbum | Rescatar esa vida | Finalizar y guardar su historial |
| PET: vista previa sin escritura | Predicción, consumo o ahorro no disponible | Solo vista previa; no se puede guardar |
| Tarjetas, vidas y tiempo que faltan | Izquierda, vivir, personas supervivientes o política | Cantidad restante |
| Rangos de Aventura Lectora | Salvia, sabiduría abstracta o cartas postales | Personaje sabio |
| Cobrar experiencia en Aventura Lectora | Precio, requisitos técnicos, búsqueda o descarga | Recibir la recompensa |
| Repasos de Estudio y Vocabulario | Reseñas, opiniones o críticas publicadas | Práctica y resultados de repetición |
| Tiempo restante del libro | Tiempo para hacer una reserva | Tiempo de lectura que queda |
| Reglas de Snake en turco, actualmente sin referencia activa | No comer el alimento y evitar el propio trasero | Comer el alimento y evitar la cola |

Los casos concretos, con texto anterior, texto nuevo y significado, están en
[`context-regressions.json`](../tools_local/i18n/context-regressions.json).
El informe del auditor añade las referencias reales al código y distingue las
claves activas de las inactivas.

## Formatos y espacio

Tres contadores de Estudio reciben una `S` inglesa desde el código. Los idiomas
externos usan etiquetas de cantidad y `%.0s` para consumir ese argumento sin
mostrarlo. Se preserva el orden y tipo de todos los argumentos. No se modifican
los formatos de inglés, español ni el respaldo inglés de Ook.

Las frases se ajustaron a sus búferes UTF-8 existentes: contadores de 80 y 40 bytes,
avisos de 48 bytes, estado de preparación de 64 bytes y cuerpo de sincronización
de 192 bytes. El auditor
lee sus capacidades del código y prueba contadores hasta el máximo de un entero
positivo. No se añadieron búferes, asignaciones ni código de ejecución al firmware.

## Verificación

- Auditoría de claves, duplicados, Unicode NFC, argumentos printf y variables web
  de todos los catálogos: sin errores.
- 13 pruebas del compilador de idiomas: correctas. Se excluye únicamente la prueba
  de empaquetado completo que copia los grandes diccionarios Wordle sin cambios.
- Cargador C++ real y scripts web: lecturas correctas de los 39 idiomas.
- Textos de Estudio con printf C++: 585 casos sin truncamiento ni sufijos ingleses en los
  idiomas naturales externos.
- PET: 38.064 renderizados con las fuentes reales, en vertical y horizontal, sin
  problemas detectados de desbordamiento, recorte de glifos o glifos ausentes.
- Aventura Lectora: 235.203 renderizados en X4 Pro y 235.227 en Sticky, en
  vertical y horizontal, sin los problemas de texto que detecta el comprobador.
- Cobertura de las nueve fuentes externas, incluidas las variantes chinas y
  tailandesas: correcta.
- Regeneración con `--strip-unused --builtin-langs en,es`: los tres archivos C++
  generados son idénticos byte por byte a los anteriores. No cambia la imagen
  del firmware; se exportan los paquetes externos `.cplang`.

Estos comprobadores no prueban cada pantalla y estado de todas las apps, ni
certifican naturalidad lingüística. Las pruebas de renderizado son de host; falta
la comprobación en el dispositivo físico, incluidas las orientaciones invertidas.
La puerta nativa general `scripts_local/check.sh --tests` falló por herramientas
Unix que faltan en este Windows (`shasum`, compilador `c++` y un `python3` utilizable).
Se detuvo tras repetir esos fallos de infraestructura y no llegó a un veredicto
final. No se cuenta como una comprobación aprobada. El registro de la entrega
conserva esa limitación separada de las comprobaciones específicas anteriores.

## Mantenimiento y comprobación en el Xteink

Ejecutar desde el repositorio:

```sh
python tools_local/i18n/audit_context.py --output qa-artifacts/i18n-audit --packs qa-artifacts/i18n-audit/languages
```

El auditor no traduce automáticamente. Rechaza regresiones exactas conocidas por
idioma y clave, y genera una lista separada de señales que requieren interpretación.
Una traducción nueva puede variar la redacción; no tiene que coincidir literalmente
con la versión corregida. Hay que mantener el contexto y los argumentos.

Para probar la actualización, copiar los `.cplang` a `/languages` en la SD y
reiniciar el lector. Conservar las fuentes, teclados y diccionarios actuales.
No borrar `.crosspoint` ni las cachés de los libros. No hace falta volver a
flashear para aplicar estas correcciones externas.

Comprobar el reloj automático en chino o portugués; los avisos de guardado y vista
previa de PET; los rangos y recompensas de Aventura Lectora; los repasos de Estudio;
y los contadores de vidas en Murdle y Carnívoro. Probar vertical, horizontal y sus
orientaciones invertidas. No deben aparecer conceptos de ahorro o rescate, una
`S` inglesa añadida al contador, glifos vacíos ni texto cortado por los cambios.
Si sigue apareciendo el texto antiguo, comprobar la ubicación del paquete y
reiniciar antes de investigar la traducción.
