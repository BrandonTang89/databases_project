#!/usr/bin/env python3
"""
run_tests.py  <interaction-name>

Runs one named interaction against the debug binary and checks that the
actual output matches the expected output recorded in the interaction file.

Interaction file format
-----------------------
Lines starting with "> " are commands sent to stdin of the binary.
All other lines are expected output lines.

Permutation blocks can be used in expected output:
    <start perm>
    ... expected lines in any order ...
    <end perm>

The runner will read exactly as many actual output lines as are in the block,
then match them order-insensitively.

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
from typing import NamedTuple


# ---------------------------------------------------------------------------
# Locate important paths relative to this script's directory, which must be
# tests/.  The workspace root is therefore one level up.
# ---------------------------------------------------------------------------
TESTS_DIR = Path(__file__).resolve().parent
WORKSPACE = TESTS_DIR.parent
BINARY = WORKSPACE / "build" / "debug" / "databases_project"
RUNNING_DIR = TESTS_DIR / "running_dir"
INTERACTIONS = TESTS_DIR / "interactions"


class ExpectedEntry(NamedTuple):
    kind: str  # "line" or "perm"
    lines: list[str]
    source_line: int


def parse_interaction(path: Path) -> tuple[list[str], list[ExpectedEntry]]:
    """Return (inputs, expected spec) extracted from an interaction file."""
    inputs: list[str] = []
    expected: list[ExpectedEntry] = []
    in_perm = False
    perm_lines: list[str] = []
    perm_start_line = 0

    for line_no, raw_line in enumerate(path.read_text().splitlines(), start=1):
        if not raw_line.strip():
            continue

        if raw_line == "<start perm>":
            if in_perm:
                raise ValueError(
                    f"Nested <start perm> at line {line_no} in {path.name}"
                )
            in_perm = True
            perm_lines = []
            perm_start_line = line_no
            continue

        if raw_line == "<end perm>":
            if not in_perm:
                raise ValueError(
                    f"<end perm> without matching <start perm> at line {line_no} in {path.name}"
                )
            expected.append(ExpectedEntry("perm", perm_lines, perm_start_line))
            in_perm = False
            perm_lines = []
            continue

        if raw_line.startswith("> "):
            inputs.append(raw_line[2:])  # strip the "> " prompt
        else:
            if in_perm:
                perm_lines.append(raw_line)
            else:
                expected.append(ExpectedEntry("line", [raw_line], line_no))

    if in_perm:
        raise ValueError(
            f"Missing <end perm> for block starting at line {perm_start_line} in {path.name}"
        )

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


def perm_block_matches(expected_lines: list[str], actual_lines: list[str]) -> bool:
    """Return True if there is a one-to-one order-insensitive line match."""
    if len(expected_lines) != len(actual_lines):
        return False

    n = len(expected_lines)
    adjacency: list[list[int]] = []
    for exp in expected_lines:
        matches = [j for j, act in enumerate(actual_lines) if line_matches(exp, act)]
        if not matches:
            return False
        adjacency.append(matches)

    # Bipartite matching (Kuhn algorithm)
    assigned_to_actual = [-1] * n

    def try_assign(exp_idx: int, seen: list[bool]) -> bool:
        for act_idx in adjacency[exp_idx]:
            if seen[act_idx]:
                continue
            seen[act_idx] = True
            if assigned_to_actual[act_idx] == -1 or try_assign(
                assigned_to_actual[act_idx], seen
            ):
                assigned_to_actual[act_idx] = exp_idx
                return True
        return False

    for exp_idx in range(n):
        if not try_assign(exp_idx, [False] * n):
            return False
    return True


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

    try:
        inputs, expected_spec = parse_interaction(interaction_path)
    except ValueError as exc:
        print(f"ERROR: {exc}")
        return False

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

    passed = True
    failures: list[str] = []

    actual_idx = 0
    for entry in expected_spec:
        if entry.kind == "line":
            exp_line = entry.lines[0]
            if actual_idx >= len(actual_lines):
                passed = False
                failures.append(f"  interaction line {entry.source_line}:")
                failures.append(f"    expected: {exp_line!r}")
                failures.append("    actual:   '<missing>'")
                continue

            act_line = actual_lines[actual_idx]
            if not line_matches(exp_line, act_line):
                passed = False
                failures.append(f"  interaction line {entry.source_line}:")
                failures.append(f"    expected: {exp_line!r}")
                failures.append(f"    actual:   {act_line!r}")
            actual_idx += 1

        elif entry.kind == "perm":
            needed = len(entry.lines)
            remaining = len(actual_lines) - actual_idx
            if remaining < needed:
                passed = False
                failures.append(
                    f"  interaction line {entry.source_line}: perm block needs {needed} lines, only {remaining} available"
                )
                failures.append(f"    expected block: {entry.lines!r}")
                failures.append(f"    actual block:   {actual_lines[actual_idx:]!r}")
                actual_idx = len(actual_lines)
                continue

            actual_block = actual_lines[actual_idx : actual_idx + needed]
            if not perm_block_matches(entry.lines, actual_block):
                passed = False
                failures.append(
                    f"  interaction line {entry.source_line}: perm block mismatch"
                )
                failures.append(f"    expected block (unsorted): {entry.lines!r}")
                failures.append(f"    actual block (unsorted):   {actual_block!r}")
                failures.append(
                    f"    expected block (sorted):   {sorted(entry.lines)!r}"
                )
                failures.append(
                    f"    actual block (sorted):     {sorted(actual_block)!r}"
                )
            actual_idx += needed

    if actual_idx < len(actual_lines):
        passed = False
        extra_count = len(actual_lines) - actual_idx
        failures.append(f"  extra output lines: {extra_count}")
        for ln in actual_lines[actual_idx:]:
            failures.append(f"    {ln!r}")

    if passed:
        print(f"PASS  {name}  ({actual_idx} lines checked)")
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
    parser.add_argument(
        "name",
        help="Name of the interaction file in tests/interactions/, or 'all' to run all.",
    )
    parser.add_argument(
        "--print_output",
        action="store_true",
        help="Print actual output instead of comparing.",
    )

    args = parser.parse_args()

    if args.name == "all":
        interaction_files = sorted(
            [f.name for f in INTERACTIONS.iterdir() if f.is_file()]
        )
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
