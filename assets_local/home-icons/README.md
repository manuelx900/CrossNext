# Home icon sources

The PNGs are the artwork for CrossNext's Home menu. The user-supplied icons keep
their decoded RGBA pixels; ancillary metadata is omitted. Games.png is a new
imagegen controller with a compact, nearly square silhouette, rounded upper
corners and four action buttons arranged in a cross. A same-named PNG takes
precedence over the older SVG when generating `src/apps_local/ui/HomeIcons.h`.
Folder.png is an imagegen replacement with a compact body, an upper-left tab
and a uniform outline matched to the accepted controller artwork.
Plugins.png is a transparent imagegen puzzle piece with a substantial uniform
outline, used by plugin shortcuts and the Plugins Home entry.

Run `python tools_local/navigation_art/generate_home.py` after changing a source.
It composites transparency over white and creates monochrome masks at the menu's
display sizes. Device rendering reads those flash masks; it does not decode PNGs.
All icons use the same inset and centered fitting, including Games.
The source outlines are checked at the generated display sizes to keep the
straight folder borders uniform and coherent with the controller.
