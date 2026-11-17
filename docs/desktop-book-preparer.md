# CrossNext Book Preparer 2.0.2

The desktop app runs the firmware's real preparation engine in WebAssembly with
an included Node runtime. See the [current 2.0.2 build and runtime instructions](../desktop-preparer/DESKTOP.txt).

## English

Select the microSD root. The app detects the saved reading engine, font, size,
orientation, layout and XTC cover rotation. Interface and reading fonts are separate.
Choose EPUB, XTC and/or XTCH. Vector fonts support 8–22 pt; bitmap families use
their installed sizes. Reader size selects the nearest supported size per family;
all sizes prepares each family's supported sizes. Font selection affects EPUB only.

Select **New** or **Classic** to match the reader. Changing it in the desktop app
does not change the reader's setting. It prepares only that engine's pages.
Changing layout later can require new pages. Covers include the dimensions used
by all four appearances and Library. Valid existing variants are reused.

Cancel leaves completed outputs intact. Resume by running the same selection again.
Books, settings, reading progress and statistics are preserved. Only known generated
cache paths under `/.crosspoint/` are written. Safely eject the card after completion.
Keep the diagnostic folder shown after an error. Do not remove the card during work.

The Windows executable is portable and unsigned. The macOS arm64/x64 DMGs are built and verified on macOS. They are not Apple-notarized; platform security prompts may apply. Open the matching DMG and drag the app to Applications. Version 2.0.2
and cache ABI versions are independent; `engine-build.json` records provenance.

## Español

Selecciona la raíz de la microSD. El programa detecta motor, fuente, tamaño,
orientación, diseño y giro de portadas XTC guardados en el lector. La fuente de
interfaz es independiente. Elige EPUB, XTC y/o XTCH. Las fuentes vectoriales
admiten 8–22 pt y las bitmap sus tamaños instalados. «Tamaño del lector» toma el
compatible más cercano de cada familia; «Todos los tamaños» prepara los de cada
una. Las fuentes solo afectan a EPUB.

Selecciona el mismo motor **Nuevo** o **Clásico** que en el lector. Cambiar el
selector del ordenador no cambia los ajustes del dispositivo. Solo prepara las
páginas de ese motor. Otro diseño puede necesitar otra paginación. Se preparan
los tamaños de portada de las cuatro apariencias y Biblioteca, reutilizando los válidos.

Cancelar conserva lo terminado y puedes continuar con la misma selección.
Solo se escriben rutas conocidas de caché generada en `/.crosspoint/`; se conservan
libros, ajustes, progreso y estadísticas. Expulsa la tarjeta al terminar y conserva
la carpeta de diagnóstico si falla. No la retires durante el proceso.

Windows es portátil y sin firmar. Los DMG para macOS arm64/x64 se generan y verifican en macOS. No tienen notarización de Apple; pueden aparecer avisos de seguridad. Abre el DMG correspondiente y arrastra la app a Aplicaciones. La versión 2.0.2 de la aplicación
y las versiones internas de caché son independientes; `engine-build.json` registra
el origen del motor. Comprueba las cachés con el firmware correspondiente en el X4 Pro físico.
