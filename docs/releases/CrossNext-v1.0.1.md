# CrossNext v1.0.1 — Xteink X4 Pro

This update fixes wallpaper selection and provides the first version step for
testing online updates from CrossNext v1.0.0.

- **Done with no selection:** a prompt now explains that you can select an
  image or continue with **No wallpaper**. Select returns to the gallery;
  No wallpaper uses the default CrossNext sleep screen. Photos and stored
  sleep-image copies remain on the SD card. Timeout quick resume is disabled so
  ordinary idle sleep also shows the default screen. A failed settings save restores
  the previous settings in memory for the current session and reports the error.
- **All / Clear:** after every usable image is selected, All becomes Clear.
  In Spanish the labels are Todos / Quitar. Clearing only changes the pending
  selection; Done still asks before applying it. Invalid images remain excluded.

Only X4 Pro is built and packaged. The application version is 1.0.1 and the raw
online-update asset remains `CrossNext-X4PRO-OTA.bin`. Keep the v1.0.0 release
available and attach the new raw OTA image to the 1.0.1 Release.

To test, keep the reader on v1.0.0, publish the tested 1.0.1 build as the latest
stable Release, then open Settings → Device → Check for Updates. After installing,
confirm version 1.0.1, test both wallpaper fixes and check again: the same version
must not be offered a second time. Draft and prerelease entries are outside the
current stable-release update endpoint.

See [installation](../install.md) and the [v1.0.0 features and credits](CrossNext-v1.0.0.md).
Physical device installation and sleep/wake testing remain required; simulator
checks do not measure the real panel or prove a completed OTA installation.
