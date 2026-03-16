#!/usr/bin/env python3
"""
execute_part.py  <bench_N.in>

Builds the release binary, then runs it on the given input file and writes
the output to the corresponding bench_N.out file in the bench_out directory.

Usage:
    python3 execute_part.py bench_in/bench_1.in
    python3 execute_part.py bench_1.in   # also accepted

The output file is placed in bench_out/bench_N.out alongside this script.
The binary is run with its CWD set to the DBSI-2026 data directory so that
paths like data/<rel>, updates/<file>, and queries/<file> resolve correctly.
"""

import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
WORKSPACE = SCRIPT_DIR.parent  # .../databases_project
BINARY = WORKSPACE / "build" / "release" / "databases_project"
DATA_DIR = SCRIPT_DIR / "DBSI-2026"  # CWD for the binary
BENCH_IN = SCRIPT_DIR / "benchmark_parts" / "bench_in"
BENCH_OUT = SCRIPT_DIR / "benchmark_parts" / "bench_out"


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


def run_benchmark(input_path: Path, output_path: Path) -> None:
    print(f"Running {input_path.name} -> {output_path.name} ...", flush=True)

    with input_path.open("r") as fin:
        commands = fin.read()

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
    # Try in bench_in directory
    candidate = BENCH_IN / p.name
    if candidate.exists():
        return candidate
    print(f"ERROR: cannot find input file '{arg}'", file=sys.stderr)
    sys.exit(1)


def main() -> None:
    if len(sys.argv) != 2:
        print(f"Usage: python3 {Path(__file__).name} <bench_N.in>", file=sys.stderr)
        sys.exit(1)

    input_path = resolve_input(sys.argv[1])

    # Derive output path: replace .in with .out, place in bench_out/
    out_name = input_path.stem + ".out"
    output_path = BENCH_OUT / out_name

    build_release()
    run_benchmark(input_path, output_path)


if __name__ == "__main__":
    main()
