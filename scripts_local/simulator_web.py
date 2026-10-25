#!/usr/bin/env python3
"""Build and serve this checkout's browser simulator on Windows."""

import argparse
import datetime as dt
from contextlib import contextmanager
import errno
import hashlib
import http.server
import json
import os
from pathlib import Path
import subprocess
import sys
import time
import urllib.parse
import urllib.request
import webbrowser

REPO = Path(__file__).resolve().parents[1]
SITE = REPO / "site"
STATE = REPO / ".pio" / "build" / "wasm" / "local-state.json"
BUILD_LOCK = REPO / ".pio" / "local-build.lock"
ARTIFACTS = ("crossplay.js", "crossplay.wasm", "crossplay.data")
SERVER_ID = hashlib.sha256(str(REPO).encode("utf-8")).hexdigest()
PAGE = """<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CrossNext local simulator</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#ecece9;color:#171717;font:16px system-ui,sans-serif}
main{display:flex;flex-direction:column;align-items:center;gap:12px;padding:16px}
h1{font-size:20px;margin:0}p{margin:0;text-align:center}
#device{position:relative;width:min(480px,calc(100vw - 32px),calc((100vh - 170px)*.6));aspect-ratio:3/5;background:white;outline:1px solid #777}
#device[data-orient=landscape]{width:min(800px,calc(100vw - 32px));aspect-ratio:5/3}
.device-canvas{position:absolute;inset:0;display:block;width:100%;height:100%;touch-action:none}.device-canvas:focus-visible{outline:3px solid #555}
nav{display:flex;gap:8px;flex-wrap:wrap;justify-content:center}button{font:inherit;min-width:48px;min-height:44px;cursor:pointer}
small{max-width:600px;text-align:center}
</style>
<main><h1>CrossNext local simulator</h1><p id="status" role="status">Loading firmware…</p>
<div id="device"></div><nav aria-label="Device controls">
<button id="back" disabled>Back</button><button id="up" disabled aria-label="Up">↑</button>
<button id="down" disabled aria-label="Down">↓</button></nav>
<small>Click or swipe the screen. Keyboard: Up/Down arrows. Simulator data resets when you reload.</small></main>
<script src="/assets/emulator.js"></script><script>
const status=document.getElementById('status');
Crossplay.boot(document.getElementById('device'),{onFirstFrame(){status.textContent='Firmware running';}})
.then(device=>{
  document.querySelectorAll('button').forEach(button=>button.disabled=false);
  document.getElementById('back').addEventListener('click',()=>{
    device.swipeBack();device.canvas.focus({preventScroll:true});
  });
  for(const key of ['up','down'])device.bindButton(document.getElementById(key),device.scancodes[key]);
  device.canvas.focus({preventScroll:true});
}).catch(error=>{status.textContent='Could not start: '+error.message;console.error(error);});
</script></html>"""


def source_digest():
    digest = hashlib.sha256()
    digest.update(("showcase-day:" + dt.date.today().isoformat()).encode("ascii"))
    roots = ("src", "lib", "sim-stubs", "freeink-sdk/libs", "tools_local/wasm", "tools_local/pet", "tools_local/intercept", "tools_local/quest", "scripts",
             "assets_local/reader-fonts/sd-card/fonts", "assets_local/sd-resources", "assets_local/quest/encoded", "assets_local/pet", "assets_local/intercept",
             "assets_local/simulator/showcase-books", "assets_local/simulator/showcase-progress",
             ".pio/libdeps/simulator_x4_pro")
    files = [REPO / "platformio.ini", REPO / "platformio.sim.ini", REPO / "tools_local/build_app_resources.py"]
    files.extend(REPO / "scripts_local" / name for name in ("sim_catchup.py", "sim_host_libs.py", "sconsign_per_tree.py"))
    for root in roots:
        files.extend(p for p in (REPO / root).rglob("*") if p.is_file() and "__pycache__" not in p.parts)
    for path in sorted(files):
        digest.update(path.relative_to(REPO).as_posix().encode("utf-8"))
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


def artifact_digests():
    return {name: hashlib.sha256((SITE / "emulator" / name).read_bytes()).hexdigest() for name in ARTIFACTS}


def remember_build():
    STATE.parent.mkdir(parents=True, exist_ok=True)
    STATE.write_text(json.dumps({"source": source_digest(), "artifacts": artifact_digests()}, indent=2), encoding="utf-8")


def current_build():
    try:
        state = json.loads(STATE.read_text(encoding="utf-8"))
        return state["source"] == source_digest() and state["artifacts"] == artifact_digests()
    except (OSError, ValueError, KeyError):
        return False


def build():
    if os.name != "nt":
        raise RuntimeError("This launcher uses the Windows tools installed in .venv.")
    python = REPO / ".venv/Scripts/python.exe"
    pio = REPO / ".venv/Scripts/platformio.exe"
    emscripten = REPO / ".venv/emsdk/upstream/emscripten"
    if not all(path.exists() for path in (python, pio, emscripten / "em++.exe")):
        raise RuntimeError("The local PlatformIO/Emscripten tools in .venv are missing.")
    # Compilation database only: browser builds replace SDL with the existing stub.
    main = (REPO / "platformio.ini").read_text(encoding="utf-8")
    sim = (REPO / "platformio.sim.ini").read_text(encoding="utf-8")
    extra = "extra_configs = platformio.sim.ini"
    sdl = "  !sdl2-config --cflags --libs\n"
    if main.count(extra) != 1 or sim.count(sdl) != 1:
        raise RuntimeError("The simulator configuration changed; review the Windows browser configuration.")
    config = REPO / ".pio/browser-platformio.ini"
    config.parent.mkdir(parents=True, exist_ok=True)
    config.write_text(main.replace(extra, "") + "\n" + sim.replace(sdl, ""), encoding="utf-8", newline="\n")
    env = os.environ.copy()
    env["PYTHONUTF8"] = "1"
    env["PATH"] = str(emscripten) + os.pathsep + env.get("PATH", "")
    subprocess.run([str(pio), "run", "-d", str(REPO), "-c", str(config), "-e", "simulator_x4_pro", "-t", "compiledb"],
                   cwd=REPO, env=env, check=True)
    subprocess.run([str(python), "-u", str(REPO / "tools_local/wasm/build.py")], cwd=REPO, env=env, check=True)
    remember_build()


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(SITE), **kwargs)

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-CrossPlay-Simulator", SERVER_ID)
        file = Path(self.translate_path(self.path))
        path = file.relative_to(SITE).as_posix()
        if file.is_file() and (path.startswith(("emulator/", "pyodide/")) or path == "study/NotoSansCJK.otf"):
            self.send_header("Content-Encoding", "br")
        super().end_headers()

    def page(self, body):
        data = PAGE.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        if body:
            self.wfile.write(data)

    def do_GET(self):
        if urllib.parse.urlsplit(self.path).path == "/":
            self.page(True)
        else:
            super().do_GET()

    def do_HEAD(self):
        if urllib.parse.urlsplit(self.path).path == "/":
            self.page(False)
        else:
            super().do_HEAD()


def existing_server(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, method="HEAD"), timeout=1) as response:
            return response.headers.get("X-CrossPlay-Simulator") == SERVER_ID
    except OSError:
        return False


@contextmanager
def build_lock():
    BUILD_LOCK.parent.mkdir(parents=True, exist_ok=True)
    with BUILD_LOCK.open("a+b") as handle:
        if os.name == "nt":
            import msvcrt
            if os.fstat(handle.fileno()).st_size == 0:
                handle.write(b"\0")
                handle.flush()
            handle.seek(0)
            waiting = False
            while True:
                try:
                    msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
                    break
                except OSError as error:
                    if error.errno not in (errno.EACCES, errno.EAGAIN, errno.EDEADLK):
                        raise
                    if not waiting:
                        print("Waiting for this checkout's simulator build…", flush=True)
                        waiting = True
                    time.sleep(0.2)
        else:
            import fcntl
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX)
        try:
            yield
        finally:
            if os.name == "nt":
                handle.seek(0)
                msvcrt.locking(handle.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(handle.fileno(), fcntl.LOCK_UN)


def prepare_server(port, force=False):
    with build_lock():
        if force or not current_build():
            print("Building this checkout's firmware for the browser…", flush=True)
            build()
        url = f"http://127.0.0.1:{port}/"
        if existing_server(url):
            return url, None
        return url, http.server.ThreadingHTTPServer(("127.0.0.1", port), Handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rebuild", action="store_true", help="Recompile before starting")
    parser.add_argument("--no-browser", action="store_true", help="Keep the server running without opening a browser")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    url, server = prepare_server(args.port, args.rebuild)
    if server is None:
        print(f"Simulator already running: {url}", flush=True)
        if not args.no_browser:
            webbrowser.open(url)
        return
    with server:
        print(f"Simulator: {url}\nKeep this window open. Ctrl+C stops the server.", flush=True)
        if not args.no_browser:
            webbrowser.open(url)
        server.serve_forever()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"Simulator could not start: {error}", file=sys.stderr)
        sys.exit(1)
