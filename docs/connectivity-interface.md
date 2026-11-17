# Connectivity interface and remote shortcuts

This change targets the X4 Pro. The radio fixes described in `radio-startup.md`
remain in place; these changes affect presentation, plugin eligibility and
remote shortcut dispatch.

## Screens

Wi-Fi rows use regular text and single-pixel rules. Long header titles fit
within the space between Back and battery status, using up to two lines.
Power actions use regular labels and the same two-pixel frame as framed Home,
with horizontal rules spanning the full row width.
Plugin lists reserve a separate trailing area for versions and limit the
description to two lines. Plugin details use the shared black header, compact
metadata and an explicit Details button for the full description.

Downloads show the received size and a real percentage when a length is known.
An unknown length uses a striped bar and received bytes, without inventing a
percentage. The existing cancellation, download staging and redraw throttling
remain in CatalogActivity. Presentation helpers live in apps_local.

The web File Manager uses fixed-size action targets, a flexible filename and a
separate nonwrapping size. Native browser dialogs own the file action sheet and
Add menu; existing upload/move/rename/delete dialogs retain their cleanup paths
and gain focus containment. Shared web chrome covers Files, Settings, Fonts,
Home, Plugins, Notes, wallpapers and the plugin runner. No external assets or
frontend framework are required.
Mobile navigation belongs to the Full server surface. Standalone Notes and
wallpaper pages share its styling without linking to unavailable Full routes.
The HTML generator embeds the site's canonical wallpaper conversion module,
including on Windows checkouts where the device-side link is a text file.

## Remote shortcuts

The first action, **Turn page / move forward**, uses reader page turning while
a book is open and logical next-item input in menus/apps. It deliberately does
not inherit a custom assignment on the physical side button. Previous behaves
the same way in reverse. Existing reader-only Next page remains available for
saved bindings. Help text distinguishes the two behaviors.

New shortcut IDs 128–134 belong to the host's existing one-byte shortcut field:
Previous, Back, Library, Files, Settings, brightness up and brightness down.
Existing side-action IDs remain unchanged. The button-first Bluetooth editor
extends binding action bits without growing records; its compatibility and
downgrade limits are in [Bluetooth device controls](bluetooth-device-controls.md).
The registry is constexpr; the settings activity owns the bounded row array.
The dispatcher uses the existing activity/input/frontlight owners and creates
no task or persistent allocation. Brightness changes reuse the existing
debounced settings write.
Extended commands claim a frame through MappedInputManager's shared local-input
arbitration, so touch, physical buttons and pending gestures keep priority.

## Home artwork and menu work

Supplied game menu silhouettes are normalized in `assets_local/game-menu-icons`
and packed by `generate_games.py` into native 40-pixel flash masks. The registry
keeps the same entries, order, launch functions and interaction geometry.

User-supplied PNGs take precedence over the old SVG sources. The generator
packs 26/32/40/64-pixel monochrome masks into flash. Common Home targets use
their matching size, with no PNG decoding or allocation on the device. This
avoids enlarging the previous 32-pixel masks for 40-pixel targets. The physical
display remains monochrome at its original resolution.

Shelf rows share one measured height instead of remeasuring every row for the
outer frame. The recording-display fixture preserves drawing coordinates and
actions while reducing line-height queries from 19 to 13 in the six-row dark
fixture. Settings frames reuse the existing row-layout cache, and leaving
Settings no longer reconstructs the theme already applied by its change
handler. These are CPU-work reductions; they are not device timing results.

## Verification

The floating Add and close controls override the translated-text button sizing
rule with fixed square bounds and zero native padding. The Add target is 58 px;
the close target is 44 px. Native file-action dialogs initially focus their
heading, avoiding a programmatic focus outline around the close control on
touch. Keyboard focus indicators remain visible on interactive controls.

Run the connectivity component fixture, menu-entry persistence checks,
locale/web terms checks and browser tests. `host-tests/connectivity-ui/plugins.py`
loads downloaded plugin scripts in an isolated browser with intercepted APIs.
It checks initialization and compatibility filtering, including cached Hub
catalogs. It does not authenticate to external services.

On the device, compare Home → Files/Library/Games/Apps/Settings and Back with
the same book, orientation and font. Check one settled presentation and that
the first tap after it works. Test the remote's first two choices in a book,
the Apps menu and Pomodoro; then test Back, screen shortcuts, brightness and
sleep. Check Power's regular labels and thin separators. Re-test Wi-Fi transfer,
Bluetooth reconnection, Plugin Hub install/cancel and the phone web dialogs.
