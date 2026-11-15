"""Package the built simulator for local screenshots, without the source build tools."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def package():
    destination = ROOT / "outputs/CrossNext-Simulator-2.0.0"
    destination.mkdir(parents=True, exist_ok=True)
    files = [ROOT / "site/assets/emulator.js"]
    files.extend(ROOT / "site/emulator" / name for name in ("crossplay.js", "crossplay.wasm", "crossplay.data", "BUILT_FROM"))
    files.extend(path for path in (ROOT / "site/showcase").rglob("*") if path.is_file())
    manifest = {"format": 1, "version": "2.0.0", "files": {}}
    for path in sorted(files):
        relative = path.relative_to(ROOT)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        manifest["files"][relative.as_posix()] = hashlib.sha256(target.read_bytes()).hexdigest()
    (destination / "scripts_local").mkdir(exist_ok=True)
    shutil.copy2(ROOT / "scripts_local/simulator_web.py", destination / "scripts_local/simulator_web.py")
    (destination / "simulator-bundle.json").write_text(json.dumps(manifest, indent=2), "utf-8")
    (destination / "Start-Simulator.cmd").write_text('''@echo off
setlocal
cd /d "%~dp0"
set PYTHONUTF8=1
where py >nul 2>nul
if not errorlevel 1 (
  py -3 scripts_local\\simulator_web.py --prebuilt --port 8767 %*
) else (
  python scripts_local\\simulator_web.py --prebuilt --port 8767 %*
)
if errorlevel 1 pause
''', "ascii")
    (destination / "Start-Simulator.command").write_text('''#!/bin/sh
cd -- "$(dirname -- "$0")" || exit 1
exec python3 scripts_local/simulator_web.py --prebuilt --port 8767 "$@"
''', "utf-8")
    guide = '''CrossNext 2.0.0 — simulador para capturas

Windows: abre Start-Simulator.cmd. Requiere Python 3.12 o posterior, ya instalado en el equipo de desarrollo.
Mac/Linux: python3 scripts_local/simulator_web.py --prebuilt --port 8767
Abre http://127.0.0.1:8767/ en Chrome o Edge. Mantén abierta la consola.
Si el puerto está ocupado: Start-Simulator.cmd --port 8768

El firmware inicia en inglés. “Firmware language” permite elegir los 39 idiomas.
Cada selector reinicia el ejemplo: los cambios hechos durante la sesión se descartan.
Los libros y nombres conservan su idioma original. No son traducciones de la UI.
“Open on startup”: Home y todas las apps/juegos del registro actual.
“Pet & Reading Quest scenarios”: 3 especies × 5 etapas; 3 progresos × 2 héroes.
“Save PNG”: captura únicamente la pantalla nativa, 480×800 o 800×480 según orientación.
“Clean view”: oculta controles. Escape o “Show controls” los restaura.
Navega tocando, manteniendo pulsado o deslizando. Back gesture reproduce el gesto lateral.

Libros con portadas, historial de 120 días, hábitos, tareas, notas, citas, tarjetas,
vocabulario, Focus y partidas de ejemplo. Los juegos sin partida persistente empiezan
desde su menú real y se pueden jugar. Los datos son ficticios; las reglas no cambian.
La fecha del ejemplo avanza desde la fecha mostrada por el estudio. Así Pet no enferma
por el tiempo que el paquete permanezca almacenado antes de abrirse.
El simulador no reproduce latencias de e-ink ni valida rendimiento del dispositivo.

Todo se sirve localmente, sin cambiar ninguna microSD física. Recargar restaura los ejemplos.
No necesita PlatformIO/Emscripten ni compila el firmware. No abre un servicio público.
Las fuentes y avisos de los libros están dentro de la tarjeta virtual.
'''
    (destination / "LEEME.txt").write_text(guide, "utf-8")
    archive = destination.with_name(destination.name + ".zip")
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=1) as output:
        for path in sorted(destination.rglob("*")):
            if path.is_file() and ".pio" not in path.parts and "__pycache__" not in path.parts:
                output.write(path, Path(destination.name) / path.relative_to(destination))
    print(destination)
    print(archive, archive.stat().st_size, hashlib.sha256(archive.read_bytes()).hexdigest())
    return destination


if __name__ == "__main__":
    package()
