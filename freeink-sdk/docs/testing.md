# Testing FreeInk SDK

Run these commands from the repository root. Host tests use production code
with desktop adapters or hardware stubs; they do not require a device or PlatformIO.
You need a C/C++ compiler with C++17 support, Python 3, and a POSIX shell.

## UI, books, credentials, and input

```sh
sh libs/ui/FreeInkUI/test/host/run.sh
sh libs/book/FreeInkBook/test/host/run.sh
sh libs/book/ContentProtection/test/host/run.sh
sh libs/hardware/InputManager/test/host/run.sh
sh libs/hardware/InputManager/test/host/run_metalio.sh
sh libs/hardware/HapticManager/test/host/run.sh
```

The book suite also needs `zip`. It uses `sips` or ImageMagick's `convert` for
JPEG fixtures and optionally Pillow for progressive JPEG fixtures; the related
checks skip when those converters are unavailable. It builds layout tests for
the default, SMALL, and LARGE memory profiles. Build outputs go under the system
temporary directory.

## Battery gauge

```sh
sh libs/hardware/BatteryMonitor/test/host/run.sh
```

Compiles `BatteryMonitor` and the real X3 board profile against a model BQ27220
that holds the Design Capacity load to TRM SLUUBD4A 6.1: key timing, CONFIG
UPDATE, checksummed Data Memory writes, and the X3's block re-select. Every
refused I2C transaction must still leave the gauge out of CONFIG UPDATE and
sealed, and the next start must finish the load.

## Fonts

```sh
sh libs/font/FreeInkFont/test/host/run.sh
sh libs/font/FreeInkFont/test/host/run_ligature.sh
```

Covers `FtFont::RenderOptions` (every `HintingMode` plus monochrome) against a
real bundled font, plus a minimal build with none of the three
`FREEINK_FONT_ENABLE_*` flags set to confirm existing consumers see no change
in output and unsupported requests are reported rather than silently degraded.

Covers `FtFont::ligature()`/`ligatureGlyphId()` (GSUB-based) against a real
bundled font: exact-value assertions for the five standard Latin ligatures,
the codepoint-vs-glyph-ID contract boundary, deinit()/re-init cache
invalidation, and a null-pointer safety check on `Gsub::LigatureGlyphId`.

## BLE HID host

```sh
sh libs/network/BleKeyboardHost/test/host/run.sh
```

Compiles `BleKeyboardHost` with `FREEINK_CAP_BLE_HID_HOST=1` against a fake
NimBLE and FreeRTOS layer, with the connection task on a real thread. Covers
report decode into key events, auto-reconnect, scanning and teardown order.
The fake has no radio, so pairing, link timing and memory use still need a
device.

## Display drivers

```sh
python3 libs/display/FreeInkDisplay/test/host/run_pro.py
python3 libs/display/FreeInkDisplay/test/host/run_uc8279.py
python3 libs/display/FreeInkDisplay/test/host/run_uc8279c_a4.py
python3 libs/display/FreeInkDisplay/test/host/run_uc8253_power.py
```

These compile production drivers against recording buses. The Pro suite also
compiles the facade in single- and dual-buffer modes. The checks cover transfer
sequences, grayscale uploads, refresh lifecycle, and power-state behavior for
the drivers included in each harness.

For the board-profile and display-probe regression test, follow the
[XteinkDetect host instructions](../libs/hardware/XteinkDetect/test/host/README.md).

## Validation limits

Host suites cover only the code and configurations each harness compiles. Run
consumer firmware builds for affected device and capability combinations, then
validate relevant refresh quality, peripheral behavior, and sleep/wake on hardware.
This SDK supplies sample PlatformIO configurations; firmware entry points belong
to the consumer project.

When checking dead code, distinguish private implementation details from public
SDK APIs. A public method, board profile, compatibility wrapper, or capability-gated
driver can have no in-repository callers and still be used by downstream firmware.
Check references across all configurations before removing internal symbols, and
exclude vendored dependencies and generated assets from routine source cleanup.
