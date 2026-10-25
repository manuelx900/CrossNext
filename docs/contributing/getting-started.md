# Getting Started with CrossNext

CrossNext builds firmware for Xteink X4 Pro (ESP32-S3). This distribution includes
the FreeInk SDK snapshot used by the firmware, including its local UI changes.
Keep the included `freeink-sdk/` sources when building this release.

## Obtain the sources

```sh
git clone https://github.com/manuelx900/CrossNext.git
cd CrossNext
```

The source ZIP provides the same source tree without Git history. The SDK is
included as ordinary source files; a separate SDK checkout is unnecessary.
The inherited `.gitmodules` file is not the SDK revision used by this export.

## Prerequisites

- Git, Python 3.12 or newer, and PlatformIO Core; this delivery used Core 6.2.0.
- The Python packages listed in `requirements.txt`.
- SDL2 for the native desktop simulator on Linux/macOS. For the Windows browser
  simulator, use [the Windows setup guide](../simulator-windows.md).
- A matching X4 Pro and a USB cable for physical testing or USB installation.

In a fresh development environment, create a local Python environment and install
the tools. In an already configured checkout, keep the existing environment and
build cache.

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install platformio==6.2.0 -r requirements.txt
```

On Windows PowerShell, create it with `py -m venv .venv`, then activate
`.\.venv\Scripts\Activate.ps1`. The pinned platform in `platformio.ini` supplies
the device framework dependencies on the first build.

## Select X4 Pro explicitly

The inherited default environment targets ESP32-C3 devices. Always select an
environment for X4 Pro; this release does not verify other boards.

| Environment | Purpose |
| --- | --- |
| `x4pro` | X4 Pro development firmware. |
| `gh_release_x4pro` | X4 Pro release firmware. |
| `simulator_x4_pro` | Desktop simulator with SDL2. |

```sh
pio run -e x4pro
pio run -e gh_release_x4pro
```

Run one build at a time: simultaneous builds can race shared PlatformIO packages.
In the configured integration workspace, `scripts_local/check.sh` owns the shared
build lock and additional checks. Its inherited target list may include other
boards; use the explicit commands above for this X4 Pro-only distribution.

For the Windows browser simulator, run `Start-Simulator.cmd` after completing its
[setup instructions](../simulator-windows.md#setup-from-a-source-export).

## Verify a change

Run the relevant suite under `host-tests/`, inspect the matching simulator
screens, and format changed C/C++ files through the repository wrapper:

```sh
./bin/clang-format-fix -g
```

Native and browser checks do not measure physical e-ink refresh time, power or
peak device heaps. Validate the affected behavior on X4 Pro before publishing a
release. [The device test plan](../interface-update.md) covers navigation,
wallpapers, light controls and image zoom.

## Installation and release files

Use [the installation guide](../install.md) to choose the application or factory
image and its correct installation method. The SD updater takes the application
image; the factory image contains bootloader and partitions.

CrossNext releases are published manually. Inherited CrossPlay workflow jobs are
restricted to their original repository; they do not provide CrossNext CI or
release builds. Online updates query CrossNext GitHub Releases and require the
raw `CrossNext-X4PRO-OTA.bin` application alias on each newer stable Release.
See [the release contract](../install.md#online-update-source) and verify a physical
X4 Pro update before publishing.

Keep the original [licence](../../LICENSE), [third-party notices](../../THIRD-PARTY-NOTICES.md)
and [upstream credits](../crossnext-changes.md). For architecture and app
interfaces, see [the architecture overview](architecture.md),
[building apps](../building-apps.md) and [the shelf contract](../shelf.md).
