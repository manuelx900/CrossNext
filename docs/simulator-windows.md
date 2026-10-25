# Local simulator on Windows

Double-click `Start-Simulator.cmd` in this checkout. It opens the current
firmware in your browser at `http://127.0.0.1:8765/`. Keep the console open;
Ctrl+C stops the server. If the same checkout already serves that port, the
launcher opens it again. Another application using the port is not stopped.
Use `Start-Simulator.cmd --port 8766` to choose another port.

Click or swipe the panel to operate it. Back performs the existing edge-swipe
gesture; the arrow buttons and keyboard Up/Down act as page buttons. Reloading
starts with the bundled demo SD card again; browser-session changes are not
written to your real device or to the bundled files.

The launcher checks source and artifact hashes before opening. It rebuilds
when they change; `Start-Simulator.cmd --rebuild` also forces the build step.
Concurrent launches wait for the same checkout's build lock and check again
before compiling, so a second launch can reuse the finished build.
The first build may take several minutes. This compiles the firmware sources
with the project's existing browser shims. Native desktop Windows is not
supported by the upstream SDL simulator; this browser path does not require WSL.

## Setup from a source export

Install Git for Windows and Python 3.12 or newer. In PowerShell, change into
the extracted CrossNext folder and create the local tools (these folders are
deliberately excluded from the source ZIP):

```powershell
py -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.2.0 -r requirements.txt brotli resvg-py zstandard
git clone https://github.com/emscripten-core/emsdk.git .venv/emsdk
.\.venv\emsdk\emsdk.bat install 6.0.6
.\.venv\emsdk\emsdk.bat activate 6.0.6
.\Start-Simulator.cmd
```

The archive includes the actual `freeink-sdk/` source tree. Do not replace it
with the older revision named by `.gitmodules`. PlatformIO downloads its own
framework dependencies on the first build. The launcher generates the browser
compilation database and bundle; no precompiled simulator is required.
The showcase save generator separately requires a C++17 compiler when its
fixtures are deliberately rebuilt; normal simulator builds only copy them.

## Tools used for this build

- PlatformIO Core 6.2.0 and Python dependencies: `.venv/Scripts/`.
- Emscripten 6.0.6, matching the project's browser CI: `.venv/emsdk/`.
- FreeInk SDK: `freeink-sdk/`, commit
  `ee59a0590408a096acddf433fcdbd6c81d592814`.

The source archive contains the SDK snapshot used by this fork, including its
local rendering changes. It does not require the original repository's Git
metadata to compile.

The Windows build generates `.pio/browser-platformio.ini` from the checked-in
configuration, removing only the native SDL flag command for compilation-database
generation. The browser already replaces SDL. The original PlatformIO files
are kept intact. `tools_local/wasm/build.py` reads Windows compiler commands
using Windows' argument parser and links through a response file to avoid the
Windows command-line length limit.

The local server binds to loopback and supplies the isolation and Brotli headers
required by the existing threaded browser build. No files are published.
Generated artifacts stay under `site/emulator/`; the launcher stores its build
fingerprint under `.pio/build/wasm/`. The demo card includes a 224px cover cache
for the new Home layout, alongside its existing 226px thumbnail.

Run the helper tests with:

```powershell
.venv\Scripts\python.exe host-tests\simulatorweb\test_simulator_web.py
```

This checks the launcher/server contracts and Windows command parsing. A browser
build and a running simulator do not validate hardware, flashing or e-ink timing.
