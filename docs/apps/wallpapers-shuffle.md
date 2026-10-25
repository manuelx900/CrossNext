# Wallpapers

Open **Apps → Wallpapers** to browse images in `/sleep`, `/WALLPAPER` and `/wallpapers` on the SD card. BMP, PNG and JPEG files are supported. The gallery shows nine images per page in a three-by-three grid. Card previews fill their frame with a centered crop, without stretching the image. The browser showcase may contain example wallpapers; the device gallery reads your card. If those folders contain no supported images, the app names the folders where images should be copied.

Tap an image to open it full screen. Hold either a thumbnail or the full-screen image to open its context menu, which shows the image name and file type/size:

- **Set wallpaper** copies that image into the existing sleep-image store and selects the appropriate custom sleep mode. The source image stays in its folder. An SD failure is reported.
- **Delete** asks for confirmation before removing the source image. A sleep-screen copy already in use is retained until another wallpaper is set. Pressing the same position again on the confirmation chooses **Keep it**.
- **Slideshow** previews images in gallery order, advancing every ten seconds. It does not change the sleep wallpaper. Tap once to stop advancing, or hold to reopen the menu.
- **Exit preview** returns to the gallery.

Back or a left-edge swipe returns from full-screen preview to the gallery. Back or a tap outside the context menu dismisses it to the screen where it was opened. The header selection icon opens multi-selection. Tap thumbnails to mark or unmark them, change pages with the pager, physical buttons or a swipe, then choose Done. Selections remain marked across gallery pages. All marks every usable image without changing the saved set, then becomes Clear (Todos / Quitar in Spanish). Clear removes the pending marks and becomes All again. Selecting every usable image manually also changes the label. Done shows the number of selected images and asks for confirmation before applying the sleep set; Cancel returns to selection with those marks intact. With no images selected, Done explains how to select an image and also offers No wallpaper. Select returns to the gallery without changing settings. Confirming No wallpaper uses the default CrossNext sleep screen and retains all source photos and sleep-image copies; an SD settings-save failure restores the previous settings in memory for the current session and reports the error. Corrupt or incomplete images cannot be selected manually or through All. Reopening the gallery scans the current card contents; selection is local to the current visit.

The existing single-image commit, sleep-mode and quick-resume checks remain in use. Confirmed multi-image sets become the random sleep set. Setting one image through its context menu replaces that selection with that image. No slideshow timer or pending selection mode is persisted and no background task is created.

On first entry, preparation appears before discovery and processes supported images with a current/total count. Cached thumbnail dimensions and source metadata are reused on later unchanged entries. The gallery balances thumbnails between the header and the page selector. Applying a chosen set reports progress; let the copy operation finish before leaving.

See the [Citas, Notas and reading UI redesign](reading-tools-redesign.md) for the shared update and verification plan.

BMP files of different supported dimensions are copied using their actual byte count. A failed copy reports a wallpaper/SD storage error and retains the previous usable set. Preparation retries failed images on the next opening.

The gallery is local-only. CrossPlay collection downloads, QR uploads and Live are absent from CrossNext. There are no Live radio checks when entering sleep or waking on a timer.
