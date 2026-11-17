# Home icon sources

The seven PNGs are the artwork supplied for CrossNext's Home menu. Their decoded
RGBA pixels are preserved; ancillary metadata is omitted. A same-named PNG takes
precedence over the older SVG when generating `src/apps_local/ui/HomeIcons.h`.

Run `python tools_local/navigation_art/generate_home.py` after changing a source.
It composites transparency over white and creates monochrome masks at the menu's
display sizes. Device rendering reads those flash masks; it does not decode PNGs.
