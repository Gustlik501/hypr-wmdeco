{
  description = "Window Maker style decorations for Hyprland 0.57";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  outputs = { self, nixpkgs }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [ "x86_64-linux" ];
      package = pkgs: pkgs.hyprlandPlugins.mkHyprlandPlugin {
        pluginName = "hypr-wmdeco";
        version = "0.1.0";
        src = self;
        nativeBuildInputs = [ pkgs.cmake ];
        buildInputs = [ pkgs.pango pkgs.cairo ];
        meta = {
          description = "Window Maker title and resize bars for Hyprland 0.57";
          homepage = "https://github.com/hyprwm/hyprland-plugins";
          license = pkgs.lib.licenses.gpl2Plus;
          platforms = [ "x86_64-linux" ];
        };
      };
    in {
      packages = forAllSystems (system: let p = package nixpkgs.legacyPackages.${system}; in {
        default = p;
        hypr-wmdeco = p;
      });
      overlays.default = final: prev: { hypr-wmdeco = package final; };
      checks = forAllSystems (system: { build = self.packages.${system}.default; });
      devShells = forAllSystems (system: let pkgs = nixpkgs.legacyPackages.${system}; in {
        default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.default ];
          packages = [ pkgs.wayland-scanner pkgs.wlr-protocols pkgs.wlrctl pkgs.foot pkgs.python3 ];
          WLR_PROTOCOLS = "${pkgs.wlr-protocols}/share/wlr-protocols";
        };
      });
    };
}
