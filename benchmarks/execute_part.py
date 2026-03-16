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

Profiling mode
--------------
Use --profile to run the benchmark under Linux perf sampling. This writes
`*.perf.data` and, when flamegraph tools are available, also writes:
`*.perf.script`, `*.folded`, and `*.flamegraph.svg` under
benchmark_parts/bench_out/profiles/.
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
WORKSPACE  = SCRIPT_DIR.parent                          # .../databases_project
BINARY     = WORKSPACE / "build" / "release" / "databases_project"
DATA_DIR   = SCRIPT_DIR / "DBSI-2026"                  # CWD for the binary
BENCH_IN   = SCRIPT_DIR / "benchmark_parts" / "bench_in"
BENCH_OUT  = SCRIPT_DIR / "benchmark_parts" / "bench_out"
PROFILE_OUT = BENCH_OUT / "profiles"


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


def resolve_tool(tool_name: str, flamegraph_dir: Path | None = None) -> str | None:
    """Resolve a tool in PATH or an optional FlameGraph directory."""
    if flamegraph_dir is not None:
        candidate = flamegraph_dir / tool_name
        if candidate.exists() and os.access(candidate, os.X_OK):
            return str(candidate)
    return shutil.which(tool_name)


def profile_base_name(output_path: Path) -> str:
    stem = output_path.stem or "benchmark_output"
    safe = "".join(ch if (ch.isalnum() or ch in "._-") else "_" for ch in stem)
    return safe


def maybe_generate_flamegraph(
    perf_data_path: Path,
    base_name: str,
    flamegraph_dir: Path | None,
) -> None:
    perf_bin = resolve_tool("perf")
    stackcollapse_bin = resolve_tool("stackcollapse-perf.pl", flamegraph_dir)
    flamegraph_bin = resolve_tool("flamegraph.pl", flamegraph_dir)

    if perf_bin is None:
        print("WARN: perf not found in PATH; cannot generate flamegraph.", flush=True)
        return
    if stackcollapse_bin is None or flamegraph_bin is None:
        print(
            "WARN: FlameGraph tools not found; keeping perf.data only.",
            flush=True,
        )
        print(
            "      Install FlameGraph scripts or pass --flamegraph-dir <dir>.",
            flush=True,
        )
        return

    PROFILE_OUT.mkdir(parents=True, exist_ok=True)
    perf_script_path = PROFILE_OUT / f"{base_name}.perf.script"
    folded_path = PROFILE_OUT / f"{base_name}.folded"
    flamegraph_svg_path = PROFILE_OUT / f"{base_name}.flamegraph.svg"

    perf_script_result = subprocess.run(
        [perf_bin, "script", "-i", str(perf_data_path)],
        capture_output=True,
        text=True,
    )
    if perf_script_result.returncode != 0:
        print("WARN: perf script failed; skipping flamegraph generation.", flush=True)
        if perf_script_result.stderr:
            print(perf_script_result.stderr, flush=True)
        return
    perf_script_path.write_text(perf_script_result.stdout)

    fold_result = subprocess.run(
        [stackcollapse_bin, str(perf_script_path)],
        capture_output=True,
        text=True,
    )
    if fold_result.returncode != 0:
        print("WARN: stackcollapse-perf.pl failed; skipping flamegraph SVG.", flush=True)
        if fold_result.stderr:
            print(fold_result.stderr, flush=True)
        return
    folded_path.write_text(fold_result.stdout)

    flame_result = subprocess.run(
        [flamegraph_bin, str(folded_path)],
        capture_output=True,
        text=True,
    )
    if flame_result.returncode != 0:
        print("WARN: flamegraph.pl failed.", flush=True)
        if flame_result.stderr:
            print(flame_result.stderr, flush=True)
        return
    flamegraph_svg_path.write_text(flame_result.stdout)
    print(f"Flamegraph written to {flamegraph_svg_path}", flush=True)


def run_benchmark(
    input_paths: list[Path],
    output_path: Path,
    profile: bool,
    profile_freq: int,
    flamegraph_dir: Path | None,
) -> None:
    names = ", ".join(p.name for p in input_paths)
    print(f"Running [{names}] -> {output_path.name} ...", flush=True)

    commands = "".join(p.read_text() for p in input_paths)

    print(f"Commands to execute:\n{commands}", flush=True)
    if profile:
        perf_bin = resolve_tool("perf")
        if perf_bin is None:
            print("ERROR: --profile requested but 'perf' is not in PATH.", file=sys.stderr)
            sys.exit(1)

        PROFILE_OUT.mkdir(parents=True, exist_ok=True)
        base_name = profile_base_name(output_path)
        perf_data_path = PROFILE_OUT / f"{base_name}.perf.data"

        print(
            f"Profiling with perf at {profile_freq} Hz -> {perf_data_path.name}",
            flush=True,
        )
        result = subprocess.run(
            [
                perf_bin,
                "record",
                "-F",
                str(profile_freq),
                "-g",
                "--call-graph",
                "dwarf",
                "-o",
                str(perf_data_path),
                str(BINARY),
            ],
            input=commands,
            capture_output=True,
            text=True,
            cwd=DATA_DIR,
        )
    else:
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

    if result.returncode != 0:
        print(f"ERROR: benchmark process exited with {result.returncode}", file=sys.stderr)
        sys.exit(result.returncode)

    if profile:
        base_name = profile_base_name(output_path)
        perf_data_path = PROFILE_OUT / f"{base_name}.perf.data"
        maybe_generate_flamegraph(perf_data_path, base_name, flamegraph_dir)


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
    parser.add_argument(
        "--profile",
        action="store_true",
        help="Run benchmark under perf and emit profiling artifacts.",
    )
    parser.add_argument(
        "--profile-freq",
        type=int,
        default=999,
        metavar="HZ",
        help="Perf sampling frequency when --profile is set (default: 999).",
    )
    parser.add_argument(
        "--flamegraph-dir",
        type=Path,
        default=None,
        metavar="DIR",
        help="Directory containing stackcollapse-perf.pl and flamegraph.pl.",
    )
    args = parser.parse_args()

    input_paths = [resolve_input(a) for a in args.inputs]
    output_path = Path(args.output) if args.output else default_output(input_paths)

    if args.profile_freq <= 0:
        print("ERROR: --profile-freq must be > 0", file=sys.stderr)
        sys.exit(1)

    flamegraph_dir = args.flamegraph_dir
    if flamegraph_dir is not None:
        flamegraph_dir = flamegraph_dir.resolve()

    build_release()
    run_benchmark(
        input_paths,
        output_path,
        profile=args.profile,
        profile_freq=args.profile_freq,
        flamegraph_dir=flamegraph_dir,
    )


if __name__ == "__main__":
    main()
