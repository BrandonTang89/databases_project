# Databases Mini-Project

## Build and Run
### Build and Run with Nix
To ensure reproducibility, I chose to use [Nix](https://nixos.org/) to manage dependencies and build the project.

Nix flakes are a way to ensure we both have the *exact* same version of all dependencies. 

It gives a shell with all the dependencies in the path.
- Install nix: https://nixos.org/download.html
- `nix develop --experimental-features 'nix-command flakes'` to enter the development environment

Now, we just use
- `build release` to build the project
- `run release` to both build and run the project

These internally call the relevant CMake build commands.

### Build and Run without Nix
I believe if you have the right GCC version installed, the following should also just work:
- `cmake --build --preset release -j 8` to build the project
- `./build/release/databases_project` to run the project

## Testing
We have 2 types of testing processes:
- Simulations: These play a series of commands from a file, waiting for each output before sending the next command. These allow easy debugging. 
- Integration Tests: These run a series of commands from a file all at once, and compare the stdout to expected values. These allow us to easily check for regressions.

The test files are in `tests/interactions`. 

```bash
build debug && python3 -m tests.simulator add_add_interact # simulation input from this file
build debug && python3 -m tests.run_tests add_add_interact # test this interaction
build debug && python3 -m tests.run_tests all # special command to run all simulations
```


## To Do
- Lock whole relation (special case for the first relation of a query)
- The rest of the matches of the query atom currently being processed
- Testing for query
- Everything to do with the conflict graph