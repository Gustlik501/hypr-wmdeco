# hypr-wmdeco

Window Maker 0.96-style titlebars and resize handles for **Hyprland 0.57.0 / Lua**.
`nix build` installs `$out/lib/libhypr-wmdeco.so` and `$out/share/doc/hypr-wmdeco/`.
`nix flake check` checks the package build. No desktop settings are applied.

Based on hyprbars **v0.57.0** (BSD-3-Clause, Hypr Development / Vaxry) and
Window Maker 0.96.0 defaults, masks and relief algorithms (GPL-2.0-or-later;
wraster originally LGPL-2.0-or-later). Combined plugin: GPL-2.0-or-later.
[Source details and deliberate differences](SOURCES.md); licences are included.

## NixOS / Home Manager

```nix
# Consuming flake inputs:
inputs.hypr-wmdeco.url = "path:/data/SSD1/dotfilesprojects/hypr-wmdeco";
inputs.hypr-wmdeco.inputs.nixpkgs.follows = "nixpkgs";
# Module, with inputs passed through specialArgs / extraSpecialArgs:
nixpkgs.overlays = [ inputs.hypr-wmdeco.overlays.default ];
# Home Manager (the explicit .so path avoids package-name inference):
wayland.windowManager.hyprland = {
  configType = "lua";
  plugins = [ "${pkgs.hypr-wmdeco}/lib/libhypr-wmdeco.so" ];
  settings.config.plugin.wmdeco.enabled = true;
};
```

Package attributes: `packages.x86_64-linux.{default,hypr-wmdeco}`;
overlay attribute: `pkgs.hypr-wmdeco`. For NixOS without Home Manager, add the
package to `environment.systemPackages`, then load it in your Lua config:

```lua
-- Use your package's actual store path; don't also load through HM's plugins.
hl.plugin.load("/nix/store/…-hypr-wmdeco-0.1.0/lib/libhypr-wmdeco.so")
hl.config({ plugin = { wmdeco = {
    enabled = true,
    font_family = "Helvetica", -- or "TeX Gyre Heros"
} } })
```

This is built with the pinned nixpkgs `hyprlandPlugins.mkHyprlandPlugin`, including
its compiler and Hyprland headers. The lock pins nixpkgs
`885ba4d038f10c3d17d7a92e8c3cc185b99c9158`. Rebuild together with your compositor
when changing nixpkgs or overriding Hyprland/dependencies; version numbers alone
are not an ABI guarantee. Initialization refuses a header/runtime hash mismatch.
Do not run alongside hyprbars or another titlebar plugin.

## Live Modern / Retro toggle

Hyprland **0.57 Lua uses `hyprctl eval`**, not the old `keyword` interface:

```sh
hyprctl eval 'hl.plugin.wmdeco.disable()'  # Modern: no bars, no reserved space
hyprctl eval 'hl.plugin.wmdeco.enable()'   # Retro
hyprctl eval 'hl.plugin.wmdeco.toggle()'
# Equivalent config update, also live:
hyprctl eval 'hl.config({plugin={wmdeco={enabled=false}}})'
```

No restart or unload is required; existing and new windows update. Lua wrappers
write the same config value, not a separate override. Reload restores the value
in your config file. To test, use **`testbed ctl eval …`** instead of bare hyprctl.
For a complete Modern/Retro toggle, separately restore your modern border,
rounding, shadow and blur settings: this plugin never changes them.

## Options

Every key below is under `hl.config({ plugin = { wmdeco = { … } } })`, internally
`plugin:wmdeco:<key>`. All update live, including geometry, fonts and exclusion.
Sizes are **logical pixels**. Fontconfig supplies missing font families; install
`pkgs.tex-gyre` and select `TeX Gyre Heros` for an explicit Helvetica substitute.
Colors accept normal Hyprland color strings (`"rgb(aaaaaa)"`); WM textures are
opaque RGB, so alpha is ignored. Gradients are continuous across title/buttons
when button texture is `inherit`.

| Key | Default | Meaning |
|---|---|---|
| `enabled` | `true` | Enable both decorations and reserved space |
| `font_family` | `"Helvetica"` | Fontconfig family/fallback |
| `font_size` | `12` | Absolute pixel size, 1–96 |
| `font_weight` | `700` | Numeric Pango weight, 100–1000 (700 = bold) |
| `title_height` | `0` | 0 = font ascent+descent + 8; otherwise 1–256 |
| `button_size` | `0` | 0 = title height; otherwise capped at title height |
| `button_padding` | `0` | Outer button inset, 0–64 |
| `title_padding` | `6` | Text inset within middle segment, 0–128 |
| `title_justify` | `"center"` | `left`, `center`, `right` |
| `resize_height` | `8` | Bottom bar height; 0 disables it; max 128 |
| `resize_corner_width` | `28` | Corner width, 0–512; shrinks to keep 20px middle |
| `double_click_ms` | `250` | Double-click interval; 0 disables; max 2000 |
| `exclude_tags` | `"games"` | Comma/whitespace-separated window tags; empty = none |
| `minimize_dispatcher` | see below | Lua dispatcher expression; empty disables |
| `double_click_dispatcher` | see below | Lua dispatcher expression; empty disables |

For each state **`focused`**, **`unfocused`**, **`owner`**, replace `<state>` below:

| Key pattern | Default |
|---|---|
| `<state>_texture` | `"solid"` |
| `<state>_color1`, `<state>_color2` | focused `#000000`, unfocused `#aaaaaa`, owner `#666666` |
| `<state>_text_color` | focused white, unfocused black, owner white |
| `<state>_button_texture` | `"inherit"` (use that state's title texture) |
| `<state>_button_color1`, `<state>_button_color2` | Same defaults as title colors; ignored with `inherit` |

`owner` is an ancestor/owner of the focused transient window, not every window
with the same app-id. Texture types: `solid`, `hgradient`, `vgradient`, `dgradient`;
buttons additionally accept `inherit`. A solid uses color1; a gradient uses both.

| Key | Default |
|---|---|
| `resize_texture` | `"solid"` |
| `resize_color1`, `resize_color2` | `#aaaaaa` |

Dispatcher defaults (not shell commands and not legacy dispatcher syntax):

```lua
minimize_dispatcher = 'hl.dsp.window.move({ workspace = "special:minimized", follow = false })'
double_click_dispatcher = 'hl.dsp.window.fullscreen({ mode = "maximized", action = "toggle" })'
```

These implement `movetoworkspacesilent special:minimized` and `fullscreen 1` in
Lua-era Hyprland. To retrieve minimized windows, bind e.g.
`hl.dsp.workspace.toggle_special("minimized")` (or move them back with your window
picker). Click/release inside left button minimizes, right button closes.
Title drag moves; double-click maximizes. Bottom left/right drag from that
corner; middle drag changes only the bottom edge. Tiled resize is delegated to
Hyprland's layout, without forcing the window floating. Only left mouse input
is handled; touch/stylus-specific gestures are not implemented.

### Per-window exclusion and purple example

```lua
hl.window_rule({ match = { class = "^example-game$" }, ["wmdeco:no_bar"] = true })
-- Also honors decorate=false and the exclude_tags list (default games).
-- Fullscreen windows always have no bars; maximized windows retain them.
hl.config({ plugin = { wmdeco = {
    focused_texture = "hgradient",
    focused_color1 = "rgb(9f79ee)", focused_color2 = "rgb(433364)",
    unfocused_texture = "hgradient",
    unfocused_color1 = "rgb(33263f)", unfocused_color2 = "rgb(110c18)",
    unfocused_text_color = "rgb(ffffff)",
} } })
```

### Recommended retro settings (outside the plugin)

```lua
hl.config({
    general = { border_size = 1,
        ["col.active_border"] = "rgb(000000)",
        ["col.inactive_border"] = "rgb(000000)" },
    decoration = { rounding = 0, blur = { enabled = false },
        shadow = { enabled = false } },
})
```

Hyprland owns the 1px frame border. The plugin adds only WM's texture relief.
Client-side decorations are not stripped; prefer SSD-capable clients such as
foot, or disable CSD using each application's supported settings.

## Performance and testing

One cached GPU texture per decoration/window, with no blur or animations added.
Rasterization/upload happens only when title, state, size, scale, pressed state
or config changes. Fullscreen/disabled/excluded windows enqueue no render passes.
`hl.plugin.wmdeco.stats()` returns cumulative `paint_count`, `render_count`,
effective `title_height` and `enabled`. Use `repl` to print return values
(`eval` only acknowledges execution):

```sh
hyprctl repl 'local s=hl.plugin.wmdeco.stats(); return s.paint_count,s.render_count,s.title_height,s.enabled'
```

[TESTING.md](TESTING.md) records isolated testbed screenshots, interactions,
limitations and how to build the guarded virtual pointer helper. **Never load
an untested plugin into an important running compositor.**
