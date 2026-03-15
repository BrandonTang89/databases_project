#!/usr/bin/env python3
"""
run_tests.py  <interaction-name>

Runs one named interaction against the debug binary and checks that the
actual output matches the expected output recorded in the interaction file.

Interaction file format
-----------------------
Lines starting with "> " are commands sent to stdin of the binary.
All other lines are expected output lines.

The token <NUMBER> in an expected line matches any sequence of digits in the
corresponding position of the actual output.

The binary is run with its cwd set to tests/running_dir so that CSV file
paths in the interaction resolve correctly.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path


# ---------------------------------------------------------------------------
# Locate important paths relative to this script's directory, which must be
# tests/.  The workspace root is therefore one level up.
# ---------------------------------------------------------------------------
TESTS_DIR    = Path(__file__).resolve().parent
WORKSPACE    = TESTS_DIR.parent
BINARY       = WORKSPACE / "build" / "debug" / "databases_project"
RUNNING_DIR  = TESTS_DIR / "running_dir"
INTERACTIONS = TESTS_DIR / "interactions"


def parse_interaction(path: Path) -> tuple[list[str], list[str]]:
    """Return (inputs, expected_outputs) extracted from an interaction file."""
    inputs: list[str] = []
    expected: list[str] = []

    for raw_line in path.read_text().splitlines():
        if not raw_line.strip():
            continue
        if raw_line.startswith("> "):
            inputs.append(raw_line[2:])   # strip the "> " prompt
        else:
            expected.append(raw_line)

    return inputs, expected


def line_matches(expected: str, actual: str) -> bool:
    """
    Return True if *actual* matches *expected*, where every occurrence of
    the literal token <NUMBER> in *expected* matches one or more digits in
    *actual*.
    """
    if "<NUMBER>" not in expected:
        return expected == actual

    # Build a regex from the expected line, escaping everything except
    # <NUMBER> which becomes \\d+.
    parts = expected.split("<NUMBER>")
    pattern = r"\d+".join(re.escape(p) for p in parts)
    return re.fullmatch(pattern, actual) is not None


def run_interaction(name: str, print_output: bool = False) -> bool:
    """
    Run the named interaction.  Returns True on pass, False on failure.
    Prints a diff-style report on mismatch.
    """
    interaction_path = INTERACTIONS / name
    if not interaction_path.exists():
        print(f"ERROR: interaction file not found: {interaction_path}")
        return False

    if not BINARY.exists():
        print(f"ERROR: binary not found: {BINARY}")
        print("       Build the project first (cmake --build build).")
        return False

    inputs, expected_lines = parse_interaction(interaction_path)

    stdin_text = "\n".join(inputs) + "\n" if inputs else ""

    result = subprocess.run(
        [str(BINARY)],
        input=stdin_text,
        capture_output=True,
        text=True,
        cwd=str(RUNNING_DIR),
    )

    if print_output:
        print(result.stdout, end="")
        if result.stderr:
            print(result.stderr, file=sys.stderr, end="")
        return True

    actual_lines = result.stdout.splitlines()

    # ------------------------------------------------------------------
    # Compare line by line
    # ------------------------------------------------------------------
    max_len = max(len(expected_lines), len(actual_lines))
    passed = True
    failures: list[str] = []

    for i in range(max_len):
        exp = expected_lines[i] if i < len(expected_lines) else "<missing>"
        act = actual_lines[i]   if i < len(actual_lines)   else "<missing>"
        if not line_matches(exp, act):
            passed = False
            failures.append(f"  line {i + 1}:")
            failures.append(f"    expected: {exp!r}")
            failures.append(f"    actual:   {act!r}")

    if passed:
        print(f"PASS  {name}  ({len(expected_lines)} lines checked)")
    else:
        print(f"FAIL  {name}")
        for msg in failures:
            print(msg)
        if result.stderr:
            print("  stderr:")
            for ln in result.stderr.splitlines():
                print(f"    {ln}")

    return passed


def main() -> None:
    parser = argparse.ArgumentParser(description="Run a database interaction test.")
    parser.add_argument("name", help="Name of the interaction file in tests/interactions/, or 'all' to run all.")
    parser.add_argument("--print_output", action="store_true", help="Print actual output instead of comparing.")
    
    args = parser.parse_args()

    if args.name == "all":
        interaction_files = sorted([f.name for f in INTERACTIONS.iterdir() if f.is_file()])
        if not interaction_files:
            print(f"No interactions found in {INTERACTIONS}")
            sys.exit(0)
            
        all_ok = True
        for name in interaction_files:
            if not run_interaction(name, args.print_output):
                all_ok = False
        sys.exit(0 if all_ok else 1)
    else:
        ok = run_interaction(args.name, args.print_output)
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
