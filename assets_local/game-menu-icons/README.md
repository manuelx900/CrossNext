# Game menu artwork

These PNG silhouettes are the supplied artwork for the Shelf game rows. Checkers
uses the final two-piece drawing rather than the earlier single-piece alternative.
Othello retains its existing navigation-family drawing; no replacement was supplied.

Run `python tools_local/navigation_art/generate_games.py` from the repository root
to regenerate `src/apps_local/ui/GameMenuIcons.h`. Generation composites alpha on
white, crops the artwork's ink bounds, preserves aspect ratio, fits a shared
34-pixel extent within a 40-pixel canvas, and packs a monochrome flash mask.
Framed Shelf rows draw those masks at their native size. Other existing row sizes
use the SDK's contain rendering. Source pixels are preserved without ancillary
PNG metadata. No PNG decoder, SD read, allocation or task is added to navigation.
