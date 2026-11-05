# Isolated test record

Tested on the NVIDIA RTX 2060 using retro-testbed's **offscreen Wayland output
inside its private headless Sway helper**, with unmodified pinned Hyprland
0.57.0 and its matching plugin headers. Native Aquamarine headless allocation
failed on this GPU; the testbed handles that separately. No live compositor,
Noctalia instance, settings or input devices were modified.

## Results

Screenshots are local, intentionally gitignored, under
`/data/SSD1/dotfilesprojects/hypr-wmdeco/.work/screens/`:

| File | Verified |
|---|---|
| `floating.png` | Focused black and unfocused grey titlebar, same-texture square buttons, WM resizebar |
| `tiled.png` | Two tiled windows with reserved top/bottom extents |
| `long-title.png` | First-word + three dots + longest fitting tail truncation |
| `gradient.png` | Continuous purple hgradient across title and buttons |
| `toggle-off.png`, `toggle-on.png` | Live disable/enable on existing floating clients |
| `tiled-toggle-off.png`, `tiled-toggle-on.png` | Tiled clients gain/return all 34px of space (26px auto title + 8px resize in test font fallback) |
| `resized-middle.png` | Floating bottom-edge resize |
| `tiled-resized.png` | Bottom corner resizes the dwindle split |
| `maximized.png`, `fullscreen.png` | Double-click maximizes with bars; true fullscreen removes them |
| `no-bar-rule.png`, `excluded-tag.png` | Dynamic wmdeco:no_bar and games tag exclusion |
| `button-pressed.png` | Held left button uses sunken texture relief |
| `texture-variants.png` | Vertical title, diagonal button override and diagonal resize texture |
| `custom-geometry.png` | Live font family/size/weight, title height, button size/inset changes |
| `live-title-change.png` | Title changes repaint even without client pixel damage |

Virtual-pointer tests (testbed display only):

- Middle drag: 500x280 -> 500x340, position unchanged.
- Left corner: x=70 -> 40, size 500x340 -> 530x370.
- Right corner: 530x370 -> 570x400, position unchanged.
- Title drag moves the window without resizing.
- Double title click produces maximized internal state 1.
- Mini button moves its window to `special:minimized` silently.
- Close button removes its window from `clients`.
- Tiled corner changes the split widths 610/610 -> 660/560; no forced float.
- `hl.config` enabled update and all three Lua toggle functions work without
  restart. Tiled client heights change 724 -> 758 -> 724 at fixed output size.
- Texture cache counters: five floating moves increased render_count 12 -> 26
  while paint_count stayed at 10. Idle snapshots also left paint_count unchanged.
  A fullscreen screenshot left both paint_count and render_count unchanged.
- A title-only OSC update increments paint_count by exactly one (41 -> 42),
  repainting the title but retaining the unchanged resize texture.
- Plugin unload/reload with existing clients succeeds in the private testbed.
- `configerrors` is empty. Repeated clients, resizes, config updates and toggles
  leave the private compositor responsive; its rolling log has no plugin crash,
  exception or segfault. Initial helper/bootstrap warnings are testbed-specific.

Visual comparison was against Window Maker's source geometry/algorithms and the
provided shape reference, not a side-by-side running X11 WM pixel-diff test.
Owner-of-focused transient state, fractional scaling/mixed-DPI, Xwayland,
complex popups, touch/stylus and real gaming frame-time performance are not
verified. Touch-specific handling is not implemented. Native CSD removal is
not implemented. Font output depends on the installed fontconfig fallback;
select TeX Gyre Heros explicitly if reproducible Helvetica-like metrics matter.

## Reproduce safely

From this project, build the plugin, then use an installed retro-testbed binary:

```sh
nix build
export TESTBED_DIR="$PWD/.work/compositor"
TB=/data/SSD1/dotfilesprojects/retro-testbed/result/bin/testbed
# Do not use another job's TESTBED_DIR!
$TB up --backend offscreen --outputs 1 --size 1280x800 \
  --config "$PWD/tests/retro.lua" --plugin "$PWD/result/lib/libhypr-wmdeco.so"
$TB run foot --config=/dev/null --app-id=wmfloat-one \
  --title='Window Maker test' --window-size-pixels=500x280 \
  sh -c 'printf "PRIVATE TEST\n"; sleep 600' >.work/foot.log 2>&1 < /dev/null &
$TB ctl eval 'hl.plugin.wmdeco.disable()'
$TB ctl eval 'hl.plugin.wmdeco.enable()'
mkdir -p .work/screens
$TB shot .work/screens/manual.png TESTBED-1
$TB ctl configerrors
$TB ctl rollinglog > .work/runtime.log
$TB down
```

Build the test-only virtual pointer (its protocol XML is pinned through nixpkgs;
no generated third-party source is committed):

```sh
nix develop --command bash -c '
  mkdir -p .work
  xml="$WLR_PROTOCOLS/unstable/wlr-virtual-pointer-unstable-v1.xml"
  wayland-scanner client-header "$xml" .work/wlr-virtual-pointer-unstable-v1-client-protocol.h
  wayland-scanner private-code "$xml" .work/pointer-protocol.c
  cc tests/pointer.c .work/pointer-protocol.c -I.work \
    $(pkg-config --cflags --libs wayland-client) -o .work/pointer
'
# Width/height are the whole logical testbed desktop, followed by start coords,
# click|hold|drag, milliseconds, and optional destination coordinates.
$TB run "$PWD/.work/pointer" 1280 800 320 404 drag 300 320 464
```

The helper refuses any display name other than `testbed-wayland` and requires
`TESTBED_DIR`. Never override those guards to target a real session. Every
hyprctl example in the README must be sent through `$TB ctl` during testing.
