# Installing CrossNext

This delivery builds and packages CrossNext for Xteink X4 Pro (ESP32-S3) only.
Other inherited board environments are outside this release's verification.

| Model | Application image | Factory image |
| --- | --- | --- |
| X4 Pro | `CrossNext-X4PRO-Firmware-v2.1.0.bin` | `CrossNext-X4PRO-Firmware-v2.1.0-factory.bin` |

Download the images from the [2.1.0 release](https://github.com/manuelx900/CrossNext/releases/tag/2.1.0).
Check the attached `SHA256SUMS.txt` before installation. Back up the microSD.
New settings default to English; choose Spanish in the language settings.
An existing settings file retains its saved language.

## Update an existing compatible installation from microSD

1. Copy the **application** `.bin` to the microSD.
2. Open **Settings > System > Update firmware** on the reader.
3. Select that application image and confirm the model and update prompt.
4. Keep power stable until the update and restart finish.

The SD picker accepts the selected `.bin` path; CrossNext's filenames are valid.
Do not select `-factory.bin` here: it includes bootloader/partitions rather than
an application alone. The updater checks board identity and available slot size.
An older partition table with smaller application slots may require factory USB
installation; an SD update does not rewrite the partition table.

## Factory installation over USB

Use the **factory** image at offset `0x0`. It contains the bootloader, partition
table and application. For example, with esptool installed and the device in
download mode, use the matching model's command:

```sh
esptool.py --chip esp32s3 --baud 921600 write_flash 0x0 CrossNext-X4PRO-Firmware-v2.1.0-factory.bin
```

An application-only image belongs at `0x10000` when deliberately writing an
application over USB; writing it at `0x0` will not install a bootloader.
USB-locked devices cannot use the factory route. Do not assume an SD update can
enlarge a locked device's application partition. No verified X4 Pro
recovery procedure is supplied by this export. Use device-specific recovery
instructions from the device vendor; do not apply X4/X3 ESP32-C3 procedures
to these S3 devices.

## Online update source

On X4 Pro, open **Settings → System → Check for Updates**, connect to Wi-Fi and
wait for the release check. The firmware queries only
`https://api.github.com/repos/manuelx900/CrossNext/releases/latest`.
It compares the latest stable tag with the installed version and asks before
installing. It does not query the CrossPlay proxy or CrossPoint releases.

For each public GitHub Release, use a semantic tag such as `2.1.0` and attach
`CrossNext-X4PRO-OTA.bin` directly to the release. The packaging tool creates this
stable application-only alias alongside the versioned application and factory
images. Its bytes match the versioned application. The updater deliberately
ignores ZIPs, factory images and differently named assets. Keep the alias name
unchanged in future releases so installed devices can find it.

The updater retains chip, board and OTA-slot size checks. A reader with an older,
smaller partition table may need a factory USB installation first. Back up the SD
card and test the update on a physical X4 Pro before publishing the release.
A source repository alone is not an update: until a public Release and its raw
OTA asset exist, there is no online firmware to install. A GitHub/API connection
failure is reported on the device; it never falls back to upstream firmware.

Attach the versioned application and factory images, `CrossNext-X4PRO-OTA.bin`,
`CrossNext-Resources.zip` and `SHA256SUMS.txt` to the GitHub release. The optional
Book Preparer installers remain available with [2.0.7](https://github.com/manuelx900/CrossNext/releases/tag/2.0.7).
For a physical OTA check, use a compatible X4 Pro with an earlier CrossNext version and verify that GitHub offers the newer build.

## Required app contents

CrossNext 2.1.0 uses the same resource pack as 2.0.7.
PET and Reading Adventure need their complete artwork folders. Keep the dictionary index files bundled with their dictionaries for faster lookup. Use Book Preparer 2.0.7
for new preparation; existing compatible book caches can still be reused.

For a new installation or an update from an older release, install the resource
pack from [release 2.1.0](https://github.com/manuelx900/CrossNext/releases/tag/2.1.0).
Back up
custom fonts and dictionaries first. Leave your `images` folder intact. Replace the old bundled `apps`,
`fonts`, `languages` and `dictionaries` folders with the new package at
the microSD root; do not merge old and new packs. Keep books, `/.crosspoint/`
data and the bundled licences. Restore custom resources separately if needed,
without restoring old bundled packs. The package includes the
reader fonts/dictionaries and `/apps/reading-adventure-v2/`, `/apps/pet-v2/`, `/apps/intercept/`.
PET needs all six of its content packs; The Intercept needs `graphics.bin`.
Missing or incompatible contents show an installation notice instead of the app.

Content packs contain no personal saves. Preserve `/.crosspoint/` when updating
resources; PET and The Intercept keep their saved data separately there. These
internal folders are excluded from the Library. Files hides dot folders by
default; enabling Show hidden files can reveal them. Restart after installing.
