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
            llvmPackages_22.clang-tools
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

              find_project_root() {
                dir="$PWD"

                while [ "$dir" != "/" ]; do
                  if [ -f "$dir/flake.nix" ] && [ -f "$dir/CMakePresets.json" ]; then
                    printf '%s\n' "$dir"
                    return 0
                  fi

                  dir=$(dirname "$dir")
                done

                echo "Could not locate project root from $PWD" >&2
                return 1
              }

              project_root=$(find_project_root)

              (
                cd "$project_root"
                cmake --build --preset "$preset" -j 8
              )
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

              find_project_root() {
                dir="$PWD"

                while [ "$dir" != "/" ]; do
                  if [ -f "$dir/flake.nix" ] && [ -f "$dir/CMakePresets.json" ]; then
                    printf '%s\n' "$dir"
                    return 0
                  fi

                  dir=$(dirname "$dir")
                done

                echo "Could not locate project root from $PWD" >&2
                return 1
              }

              project_root=$(find_project_root)

              (
                cd "$project_root"
                cmake --build --preset "$preset"
              )

              binary_in_preset_dir="$project_root/$preset/databases_project"
              binary_in_build_subdir="$project_root/build/$preset/databases_project"

              if [ -x "$binary_in_preset_dir" ]; then
                binary="$binary_in_preset_dir"
              elif [ -x "$binary_in_build_subdir" ]; then
                binary="$binary_in_build_subdir"
              else
                echo "Could not find executable at $binary_in_preset_dir or $binary_in_build_subdir" >&2
                exit 1
              fi

              exec "$binary"
            '')
          ];
        };
      }
    );
}
