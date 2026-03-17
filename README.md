# Databases Mini-Project

## Documentation
The report is provided at [docs/ProjectReport_1075201.pdf](docs/ProjectReport_1075201.pdf).

## Build and Run
### With CMake, GCC and Ninja
With GCC 15.2.0, the following should just work:
- `cmake --preset release` to configure the project
- `cmake --build --preset release -j 8` to build the project
- `./build/release/databases_project` to run the project

This has been written into `compile.sh`

### With Just GCC
Run `./compile_nocmake.sh` to use a single invocation of `g++` to compile the release binary. 

Similarly it will be written to `./build/release/databases_project`.

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

These tests have 2 special features beyond line for line matching
- Allowing permutations of lines to account for possibly swapped outputs (e.g. for query results)
- Allowing for any number for the latency in the output, since this can vary widely across runs and machines

## Benchmarking
The project includes a benchmarking suite located in the `benchmarks/` directory. This suite allows for measuring the performance of the database on various operations including imports, queries, and deletions, with support for profiling.

### Benchmarking Workflow
The benchmarking workflow (as described in the assignment) involves executing a sequence of 6 steps:
1. **Import (Step 1):** Add all relations and then rollback.
2. **Import (Step 2):** Add all relations and then commit.
3. **Query (Step 3):** Run 14 queries and then commit.
4. **Delete (Step 4):** Delete relations and then rollback.
5. **Delete (Step 5):** Delete relations and then commit.
6. **Query (Step 6):** Run 14 queries and then commit.

To run the full benchmark:
```bash
python3 benchmarks/bench.py benchmarks/benchmark_parts/bench_in/bench_{1,2,3,4,5,6}.in
```
Or for a quick test using the smaller minibench which just does some loads and queries:
```bash
python3 benchmarks/bench.py benchmarks/benchmark_parts/bench_in/minibench_1.in
```

This will:
- Build the release binary.
- Concatenate the input files.
- Run the binary and output the results to `benchmarks/benchmark_parts/bench_out/benchmark_output.out`.

### Report Generation
To generate a Typst-formatted table summary of the benchmark results:
```bash
python3 benchmarks/produce_report_table.py
```
This script parses the benchmark output and calculates the time taken for each operation.

### Profiling
The `bench.py` script also supports profiling using Linux `perf`:
```bash
python3 benchmarks/bench.py --profile benchmarks/benchmark_parts/bench_in/bench_{1,2,3,4,5,6}.in
```
When `--profile` is used, it captures performance data and, if [FlameGraph](https://github.com/brendangregg/FlameGraph) tools are available in your PATH or provided via `--flamegraph-dir`, it automatically generates a flamegraph SVG in `benchmarks/benchmark_parts/bench_out/profiles/`.

Both `perf` and `flamegraph` are provided in the Nix development environment, so you can use them without additional setup when using Nix.

### Directory Structure
- `benchmarks/benchmark_parts/bench_in/`: Contains input files for each step of the benchmark.
- `benchmarks/benchmark_parts/bench_out/`: Contains benchmark results and profiles.
- `benchmarks/DBSI-2026/`: Contains the data, queries, and updates used by the benchmarks.

## Nix Flake Helpers
Use the following to get a development shell:
- `nix develop --experimental-features 'nix-command flakes'`

This provides some helper commands:
- `build release` to build the project
- `run release` to both build and run the project

These internally call the relevant CMake build commands.