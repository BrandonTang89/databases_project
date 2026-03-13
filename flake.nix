{
  description = "Databases Project Development Flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
      in
      {
        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            gcc15
            cmake
            clang-tools
            ninja
            (writeShellScriptBin "build" ''
              set -eu

              if [ "$#" -ne 1 ]; then
                echo "Usage: build <debug|release>" >&2
                exit 1
              fi

              preset="$1"
              case "$preset" in
                debug|release)
                  ;;
                *)
                  echo "Invalid preset: $preset. Expected 'debug' or 'release'." >&2
                  exit 1
                  ;;
              esac

              cmake --build --preset "$preset"
            '')
            (writeShellScriptBin "run" ''
              set -eu

              if [ "$#" -ne 1 ]; then
                echo "Usage: run <debug|release>" >&2
                exit 1
              fi

              preset="$1"
              case "$preset" in
                debug|release)
                  ;;
                *)
                  echo "Invalid preset: $preset. Expected 'debug' or 'release'." >&2
                  exit 1
                  ;;
              esac

              cmake --build --preset "$preset"

              binary="build/$preset/databases_project"

              if [ ! -x "$binary" ]; then
                echo "Could not find executable at build/$preset/databases_project or $preset/databases_project" >&2
                exit 1
              fi

              exec "$binary"
            '')
          ];
        };
      }
    );
}
