#!/usr/bin/env python3
"""
execute_part.py  [options] <bench_N.in> [<bench_M.in> ...]

Builds the release binary, concatenates the given input files in order, runs
the binary on the combined input, and writes the output file.

Single input file:
    The output is named bench_N.out and placed in benchmark_parts/bench_out/.

Multiple input files:
    The output defaults to benchmark_parts/bench_out/benchmark_output.out.
    Use -o / --output to override the output path in either case.

The binary is run with its CWD set to the DBSI-2026 data directory so that
paths like data/<rel>, updates/<file>, and queries/<file> resolve correctly.
"""

import argparse
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
WORKSPACE  = SCRIPT_DIR.parent                          # .../databases_project
BINARY     = WORKSPACE / "build" / "release" / "databases_project"
DATA_DIR   = SCRIPT_DIR / "DBSI-2026"                  # CWD for the binary
BENCH_IN   = SCRIPT_DIR / "benchmark_parts" / "bench_in"
BENCH_OUT  = SCRIPT_DIR / "benchmark_parts" / "bench_out"


def build_release() -> None:
    print("Building release binary...", flush=True)
    result = subprocess.run(
        ["cmake", "--build", "--preset", "release", "-j", "8"],
        cwd=WORKSPACE,
        capture_output=False,
    )
    if result.returncode != 0:
        print("ERROR: build failed", file=sys.stderr)
        sys.exit(1)
    print("Build succeeded.", flush=True)


def run_benchmark(input_paths: list[Path], output_path: Path) -> None:
    names = ", ".join(p.name for p in input_paths)
    print(f"Running [{names}] -> {output_path.name} ...", flush=True)

    commands = "".join(p.read_text() for p in input_paths)

    print(f"Commands to execute:\n{commands}", flush=True)
    result = subprocess.run(
        [str(BINARY)],
        input=commands,
        capture_output=True,
        text=True,
        cwd=DATA_DIR,
    )

    BENCH_OUT.mkdir(parents=True, exist_ok=True)
    output_path.write_text(result.stdout)

    if result.stderr:
        print("--- stderr ---", flush=True)
        print(result.stderr, flush=True)

    print(f"Output written to {output_path}", flush=True)


def resolve_input(arg: str) -> Path:
    """Accept either a bare name like 'bench_1.in' or a full/relative path."""
    p = Path(arg)
    if p.exists():
        return p.resolve()
    candidate = BENCH_IN / p.name
    if candidate.exists():
        return candidate
    print(f"ERROR: cannot find input file '{arg}'", file=sys.stderr)
    sys.exit(1)


def default_output(input_paths: list[Path]) -> Path:
    if len(input_paths) == 1:
        return BENCH_OUT / (input_paths[0].stem + ".out")
    return BENCH_OUT / "benchmark_output.out"


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build release binary and run benchmark input file(s)."
    )
    parser.add_argument(
        "inputs",
        nargs="+",
        metavar="bench_N.in",
        help="One or more input files (concatenated in order).",
    )
    parser.add_argument(
        "-o", "--output",
        metavar="FILE",
        default=None,
        help="Output file path (default: bench_N.out for a single input, "
             "benchmark_output.out for multiple).",
    )
    args = parser.parse_args()

    input_paths = [resolve_input(a) for a in args.inputs]
    output_path = Path(args.output) if args.output else default_output(input_paths)

    build_release()
    run_benchmark(input_paths, output_path)


if __name__ == "__main__":
    main()
