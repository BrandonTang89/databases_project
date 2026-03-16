#!/usr/bin/env python3

from __future__ import annotations

import re
from pathlib import Path

BENCHMARK_OUTPUT = Path("benchmarks/benchmark_parts/bench_out/benchmark_output.out")

COMMAND_TIME_RE = re.compile(r"^Command time:\s+(\d+)\s+ms$")
NUM_ANSWERS_RE = re.compile(r"^Number of answers:\s+(\d+)$")
TF_ROLLBACK_RE = re.compile(r"^Transaction TF\d+ was rolled back\.$")


def find_line(lines: list[str], exact: str, start: int = 0) -> int:
    for idx in range(start, len(lines)):
        if lines[idx] == exact:
            return idx
    raise ValueError(f"Could not find line: {exact}")


def command_time_at(lines: list[str], idx: int) -> int:
    match = COMMAND_TIME_RE.match(lines[idx])
    if match is None:
        raise ValueError(f"Expected command time at line index {idx}")
    return int(match.group(1))


def next_command_time(lines: list[str], after_idx: int) -> int:
    for idx in range(after_idx + 1, len(lines)):
        match = COMMAND_TIME_RE.match(lines[idx])
        if match is not None:
            return int(match.group(1))
    raise ValueError("Could not find command time after index {after_idx}")


def sum_times_for_prefix(lines: list[str], start_idx: int, end_idx: int, prefix: str) -> int:
    total = 0
    for idx in range(start_idx, end_idx):
        if lines[idx].startswith(prefix):
            total += command_time_at(lines, idx + 1)
    return total


def extract_query_stats(lines: list[str], tx_name: str) -> tuple[list[int], list[int], int]:
    tx_created = find_line(lines, f"Transaction {tx_name} was created.")
    tx_committed = find_line(lines, f"Transaction {tx_name} was committed.", tx_created + 1)

    answer_counts: list[int] = []
    query_times: list[int] = []

    for idx in range(tx_created + 1, tx_committed):
        answer_match = NUM_ANSWERS_RE.match(lines[idx])
        if answer_match is not None:
            answer_counts.append(int(answer_match.group(1)))
            query_times.append(command_time_at(lines, idx + 1))

    commit_time = next_command_time(lines, tx_committed)
    return answer_counts, query_times, commit_time


def render_table(
    step1_import: int,
    step1_rollback: int,
    step2_import: int,
    step2_commit: int,
    step3_answers: list[int],
    step3_times: list[int],
    step3_commit: int,
    step4_delete: int,
    step4_rollback: int,
    step5_delete: int,
    step5_commit: int,
    step6_answers: list[int],
    step6_times: list[int],
    step6_commit: int,
) -> str:
    lines: list[str] = [
        "table.hline(),",
        r"    table.header([], [Action], [Time (ms)], [\# of Answers]),",
        "    table.hline(),",
        f"    table.cell(rowspan: 2)[Step 1], [Import: ], [{step1_import}], [],",
        f"    [Rollback: ], [{step1_rollback}], [],",
        "    table.hline(),",
        f"    table.cell(rowspan: 2)[Step 2], [Import: ], [{step2_import}], [],",
        f"    [Commit: ], [{step2_commit}], [],",
        "    table.hline(),",
        "    table.cell(rowspan: 15)[Step 3],",
    ]

    for idx, (time_ms, answers) in enumerate(zip(step3_times, step3_answers, strict=True), start=1):
        lines.append(f"    [Query {idx}: ], [{time_ms}], [{answers}],")

    lines.extend(
        [
            f"    [Commit: ], [{step3_commit}], [],",
            "    table.hline(),",
            f"    table.cell(rowspan: 2)[Step 4], [Delete: ], [{step4_delete}], [],",
            f"    [Rollback: ], [{step4_rollback}], [],",
            "    table.hline(),",
            f"    table.cell(rowspan: 2)[Step 5], [Delete: ], [{step5_delete}], [],",
            f"    [Commit: ], [{step5_commit}], [],",
            "    table.hline(),",
            "    table.cell(rowspan: 15)[Step 6],",
        ]
    )

    for idx, (time_ms, answers) in enumerate(zip(step6_times, step6_answers, strict=True), start=1):
        lines.append(f"    [Query {idx}: ], [{time_ms}], [{answers}],")

    lines.extend(
        [
            f"    [Commit: ], [{step6_commit}], [],",
            "    table.hline(),",
        ]
    )
    return "\n".join(lines)


def main() -> None:
    lines = BENCHMARK_OUTPUT.read_text().splitlines()

    t1_created = find_line(lines, "Transaction T1 was created.")
    t1_rolled_back = find_line(lines, "Transaction T1 was rolled back.", t1_created + 1)
    t2_created = find_line(lines, "Transaction T2 was created.", t1_rolled_back + 1)
    t2_committed = find_line(lines, "Transaction T2 was committed.", t2_created + 1)

    step1_import = sum_times_for_prefix(lines, t1_created + 1, t1_rolled_back, "Tuples added:")
    step1_rollback = next_command_time(lines, t1_rolled_back)
    step2_import = sum_times_for_prefix(lines, t2_created + 1, t2_committed, "Tuples added:")
    step2_commit = next_command_time(lines, t2_committed)

    step3_answers, step3_times, step3_commit = extract_query_stats(lines, "T3")
    step6_answers, step6_times, step6_commit = extract_query_stats(lines, "T6")

    if len(step3_answers) != 14 or len(step6_answers) != 14:
        raise ValueError("Expected 14 query rows in Step 3 and Step 6")

    tf1_created = find_line(lines, "Transaction TF1 was created.", t2_committed + 1)
    t5_created = find_line(lines, "Transaction T5 was created.", tf1_created + 1)
    t5_committed = find_line(lines, "Transaction T5 was committed.", t5_created + 1)

    step4_delete = sum_times_for_prefix(lines, tf1_created + 1, t5_created, "Tuples deleted:")
    step4_rollback = 0
    for idx in range(tf1_created + 1, t5_created):
        if TF_ROLLBACK_RE.match(lines[idx]) is not None:
            step4_rollback += command_time_at(lines, idx + 1)

    step5_delete = sum_times_for_prefix(lines, t5_created + 1, t5_committed, "Tuples deleted:")
    step5_commit = next_command_time(lines, t5_committed)

    print(
        render_table(
            step1_import=step1_import,
            step1_rollback=step1_rollback,
            step2_import=step2_import,
            step2_commit=step2_commit,
            step3_answers=step3_answers,
            step3_times=step3_times,
            step3_commit=step3_commit,
            step4_delete=step4_delete,
            step4_rollback=step4_rollback,
            step5_delete=step5_delete,
            step5_commit=step5_commit,
            step6_answers=step6_answers,
            step6_times=step6_times,
            step6_commit=step6_commit,
        )
    )


if __name__ == "__main__":
    main()
