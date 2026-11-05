# Provenance and geometry

- Integration base: hyprwm/hyprland-plugins **v0.57.0**, hyprbars, BSD-3-Clause,
  Copyright (c) 2023 Hypr Development; hyprbars author Vaxry.
  Unmodified starting point is in `upstream/hyprbars/`;
  its licence is `LICENSE.hyprbars`. Source archive is pinned by nixpkgs at
  SHA256 `sha256-y+V5RFJcjnV1+ZzImkfiSJ97iDIflanjP2OR9lemvTs=`.
- Window Maker **0.96.0**, https://www.windowmaker.org/ and
  https://repo.or.cz/wmaker-crm.git/snapshot/wmaker-0.96.0.tar.gz .
  Defaults and algorithms were read from the pinned nixpkgs `windowmaker.src`.
  Nixpkgs fetches tag `wmaker-0.96.0` with source hash
  `sha256-6DS5KztCNWPQL6/qJ5vlkOup2ourxSNf6LLTFYpPWi8=`.
  Window Maker is GPL-2.0-or-later, Copyright (c) 1997-2003 Alfredo K. Kojima,
  Dan Pascu, and contributors. Wraster is originally LGPL-2.0-or-later,
  Copyright (c) 1997-2003 Alfredo K. Kojima; its adapted algorithms are used
  here under GPL-2.0-or-later as permitted by LGPL section 3.
  This combined plugin is GPL-2.0-or-later; `LICENSE` contains Window Maker's
  GPLv2 copy and `LICENSE.wraster` preserves wraster's original licence.
  No external artwork is included.

Reference locations in Window Maker:

- `WindowMaker/Defaults/WindowMaker.in`: F black/white; P gray40/white;
  U #aaaaaa/black; resize #aaaaaa; center justification; font Sans bold 12px;
  1px black frame/focused border. `NewStyle = new`.
- `src/framewin.c`, `wFrameWindowUpdateBorders`: height is font ascent+descent
  plus twice title clearance (default 0) plus twice TITLEBAR_EXTEND_SPACE (4).
  New-style square buttons equal that height, padding zero; left hides below
  four title heights, right below two. Title inset is six pixels on each side.
- `src/wconfig.h.in`: resize height 8, corner width 28, minimum center width 20.
  `framewin.c` draws paired vertical dark/light lines, NOT three fully bevelled
  rectangles; the upper two lines span the entire resize bar.
- `src/texture.c`, `wTextureMakeSolid`/`wDrawBevel`: solid raised relief uses
  HSV value *1.6 for light, value /2 for dim, black outer bottom/right.
  Black specially uses #b6b6b6 and #616161. `wrlib/misc.c` RBEV_RAISED2
  adds 80 to top/left, subtracts 40 from inner bottom/right, adds black outer
  bottom/right; this is deliberately different from solid relief.
- `src/def_pixmaps.h`: PRED_CLOSE_XPM and PRED_ICONIFY_XPM 10x10 masks.
  New-style paintButton uses the mask with the title foreground, not XPM RGB.
- `src/misc.c`, `ShrinkString`: keep first word (or colon prefix), add three
  ASCII dots, keep the longest fitting tail. Here UTF-8 boundaries are respected.

Intentional differences requested by the brief: Helvetica/fontconfig fallback
instead of Sans; pressed buttons retain the texture with sunken relief instead
of WM's white/black pushed state; Hyprland dispatchers implement minimize and
maximize, rather than X11 iconification/shading. Pango/Wayland font metrics and
fractional scaling can differ from Xft. This is not an assertion of pixel-perfect
X11 equivalence on arbitrary fonts/scales.
