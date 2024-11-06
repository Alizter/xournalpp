{
  description = "Xournal++";

  inputs = {
    flake-parts.url = "github:hercules-ci/flake-parts";
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = inputs@{ flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      # This is the list of architectures that work with this project
      systems = [
        "x86_64-linux" "aarch64-linux" "aarch64-darwin" "x86_64-darwin"
      ];
      perSystem = { config, self', inputs', pkgs, system, ... }:

      let
          dependencies = with pkgs; [
            # C++ Compiler is already part of stdenv
            boost
            catch2
            cmake
            pkg-config
            glib
            gtk3
            poppler
            libxml2
            libzip
            portaudio
            libsndfile
            librsvg
            gtksourceview4
            alsa-lib
          ];
      in
      {

        # devShells.default describes the default shell with C++, cmake, boost,
        # and catch2
        devShells.default = pkgs.mkShell {
            nativeBuildInputs = dependencies;

            packages = (with pkgs; [
                # Extra dev packages
                ninja
                clang-tools
            ]);
        };
      };
    };
}
