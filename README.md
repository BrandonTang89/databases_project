# Databases Mini-Project

## Build and Test
### Build and Test with Nix
To ensure reproducibility, I chose to use [Nix](https://nixos.org/) to manage dependencies and build the project.

Nix flakes are a way to ensure we both have *exact* same version of all dependencies. 

It gives a shell with all the dependencies in the path.
- Install nix: https://nixos.org/download.html
- `nix develop --experimental-features 'nix-command flakes'` to enter the development environment

Now, we just use
- `build release` to build the project
- `run release` to both build and run the project

These internally call the relevant CMake build commands.

### Build and Test without Nix
I believe if you have the right GCC version installed, the following should also just work:
- `cmake --build --preset release` to build the project
- `./build/release/databases_project` to run the project