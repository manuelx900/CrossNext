# Revisión ampliada de traducciones de la interfaz

Esta pasada parte del árbol unificado y conserva las correcciones anteriores.
Se han corregido **1.660 valores adicionales de 127 claves en 38 idiomas**;
**1.385** corresponden a claves referenciadas por el código actual. No se vuelven
a contar las 1.068 correcciones de la pasada inmediatamente anterior.

## Cobertura y límites

Se comprueban los **39 catálogos y sus 317.915 valores de origen**: 38 idiomas
naturales con 8.355 claves cada uno y Ook, idioma ficticio con respaldo en inglés.
La comprobación incluye las claves de lector, ajustes, biblioteca, juegos, PET,
Aventura Lectora y web. Se conservan `Male` y `Female` en inglés.

Esto combina comprobación estructural completa con correcciones semánticas
registradas. **No certifica nativamente cada frase de cada idioma.** El inventario
editorial conserva candidatos de inglés residual, nombres, cifras y rutas para
revisión con contexto. Una coincidencia puede ser legítima: `CrossPoint Sync`,
un nombre de archivo, una unidad o una flexión de un nombre propio no se deben
traducir a ciegas. Los relatos largos y frases sin revisión editorial siguen
necesitando hablantes nativos.

## Correcciones por contexto

| Uso | Fallo encontrado | Criterio aplicado |
| --- | --- | --- |
| Reanudar lectura/partida | CV, resumen | Continuar la sesión |
| Aplicar en Wordle | Solicitar empleo | Aplicar la opción |
| Peso de fuente | Frecuente, permanente, valiente | Normal y negrita |
| PET: ánimo y recursos | Dinero, pelo, comida confundida con forro | Nombre real del atributo, signo y cantidad intactos |
| PET: acariciar | Acciones románticas o sexuales, alimentar | Cuidado del animal virtual |
| PET: práctica | Profesión/aplicación, negación de desbloqueo invertida | Práctica gratuita; no desbloquea el recuerdo |
| Estudio: mazos y sincronización | Cubiertas de barco, construcción, detener al volver | Colección de tarjetas; Atrás deja continuar la sincronización |
| Estudio: gráfico | Hace dos días, futuro invertido | Dos semanas pasadas, hoy, tarjetas próximas |
| Batalla Naval | Tablero confundido con consejo/directorio | Cuadrícula del oponente; misma etiqueta en ayuda y botón |
| Cancelar selección de mazos | Afirmación global de que nada cambió | Solo la selección queda igual; los repasos previos pueden estar guardados |
| Cancelar descarga de mazos | Afirmación de que todos los mazos están intactos | Descarga detenida; volver a sincronizar, sin prometer deshacer cambios previos |
| Juegos y avisos de guardado | Frases parcialmente inglesas, capital financiero por pila de cartas | Instrucciones completas y consecuencias de guardado preservadas |
| Personaje Hooper | Arquero/español | Nombre propio conservado |

Las referencias incluyen `HomeActivity.cpp:861`, `WordleScreens.cpp:263`,
`PetGameScreens.cpp:356`, `StudyActivity.cpp:3131` y `StudyScreens.cpp:356`.
Cada valor anterior/nuevo y su significado están en
[`complete-regressions.json`](../tools_local/i18n/complete-regressions.json).
El auditor agrega las referencias actuales de cada clave y distingue las activas.
Para el vocabulario de tarjetas se contrastaron términos con el
[repositorio oficial de traducciones de Anki](https://github.com/ankitects/anki-core-i18n).
Eso orienta la terminología; no acredita toda la redacción de CrossNext.

## Espacio, memoria y compatibilidad

Los avisos se ajustan al destino real de `snprintf`, incluida la terminación UTF-8.
Los contadores externos consumen la `S` inglesa del llamador mediante `%.0s` sin
mostrarla; se conservan tipos y orden de argumentos. Los mensajes también se
prueban con las fuentes de la interfaz: caber en bytes no garantiza caber en píxeles.

`SyncFlowModel::factLines` pasa de 3×40 a 3×128 bytes. Permite conservar «rehecho
con tus respuestas» sin cortar las traducciones. Son **264 bytes adicionales por
modelo**, reutilizados dentro de la actividad existente; el simulador tiene dos
modelos y añade 528 bytes. No hay una nueva asignación por mensaje, tarea, caché,
archivo persistente ni framebuffer. El binario y los paquetes deben distribuirse
juntos: el firmware anterior conserva destinos de 40 bytes para estos mensajes.
No se cambia la calidad de las fuentes ni el motor EPUB.

## Verificación reproducible

Los comandos y límites están en
[`host-tests/locale-ui/README.md`](../host-tests/locale-ui/README.md).
La matriz PET cubre 39 idiomas, dos orientaciones, tres especies, cinco etapas y
las fases de sus minijuegos. La matriz Estudio usa sus constructores reales,
cuatro familias visuales, dos orientaciones y seis estados. El ensayo de Estudio
separa los textos editados de los diagnósticos de textos sin modificar; su verde
no certifica toda la pantalla. El modo `--strict` también falla por esos casos.

Los paquetes se contrastan con el cargador C++ real y con los scripts web
generados. Se comprueban formatos, claves, NFC, glifos y límites de contadores.
Los resultados, las revisiones independientes, el diff fijado y las compilaciones
finales se conservan con los artefactos de esta pasada.

El control nativo general de este host Windows sigue sin estar disponible por
herramientas de shell/compilador ausentes; no se presenta como aprobado. La
revisión en X4 Pro físico y la revisión editorial nativa siguen pendientes.
No se publica ni se flashea el lector automáticamente.

## Prueba en el dispositivo

Instalar el firmware y los paquetes correspondientes a esta misma entrega.
Comparar Reanudar, opciones de fuente y Aplicar en Wordle; revisar los recursos,
cuidados y reglas de Pet; abrir el gráfico y la sincronización de Estudio.
Probar árabe/persa, tailandés, ucraniano, alemán y japonés en ambas orientaciones.
Esperado: signos y cantidades correctos, ninguna `S` inglesa añadida, fechas y
avisos comprensibles y Atrás sin cancelar una sincronización ya segura. Registrar
cualquier corte con idioma, tipografía, pantalla y orientación. No borrar partidas,
libros, fuentes o cachés para probar traducciones.
