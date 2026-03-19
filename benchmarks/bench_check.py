#!/usr/bin/env python3

from __future__ import annotations

import re
import sqlite3
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
DBSI_DIR = SCRIPT_DIR / "DBSI-2026"
BENCH_IN_DIR = SCRIPT_DIR / "benchmark_parts" / "bench_in"


ATOM_RE = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\(([^)]*)\)")


def quote_ident(ident: str) -> str:
    return '"' + ident.replace('"', '""') + '"'


def parse_atoms(query_text: str) -> list[tuple[str, list[str]]]:
    atoms: list[tuple[str, list[str]]] = []
    for relation, args_text in ATOM_RE.findall(query_text):
        args = [part.strip() for part in args_text.split(",")]
        if len(args) != 2:
            raise ValueError(f"Only binary atoms are supported: {relation}({args_text})")
        atoms.append((relation, args))
    if not atoms:
        raise ValueError(f"No atoms found in query: {query_text}")
    return atoms


def query_to_count_sql(query_text: str) -> tuple[str, list[int]]:
    atoms = parse_atoms(query_text)

    from_parts: list[str] = []
    predicates: list[str] = []
    params: list[int] = []
    var_bindings: dict[str, str] = {}

    for index, (relation, args) in enumerate(atoms, start=1):
        alias = f"t{index}"
        from_parts.append(f"{quote_ident(relation)} {alias}")

        for position, arg in enumerate(args, start=1):
            col = f"{alias}.c{position}"
            if arg and arg[0].isdigit():
                predicates.append(f"{col} = ?")
                params.append(int(arg))
                continue

            existing = var_bindings.get(arg)
            if existing is None:
                var_bindings[arg] = col
            else:
                predicates.append(f"{col} = {existing}")

    where_clause = ""
    if predicates:
        where_clause = " WHERE " + " AND ".join(predicates)

    sql = "SELECT COUNT(*) FROM " + ", ".join(from_parts) + where_clause
    return sql, params


def create_relation(conn: sqlite3.Connection, relation: str) -> None:
    rel = quote_ident(relation)
    conn.execute(f"CREATE TABLE IF NOT EXISTS {rel} (c1 INTEGER NOT NULL, c2 INTEGER NOT NULL)")
    conn.execute(f"CREATE INDEX IF NOT EXISTS idx_{relation}_c1c2 ON {rel} (c1, c2)")
    conn.execute(f"CREATE INDEX IF NOT EXISTS idx_{relation}_c2c1 ON {rel} (c2, c1)")


def read_pairs(file_path: Path) -> list[tuple[int, int]]:
    pairs: list[tuple[int, int]] = []
    with file_path.open("r", encoding="utf-8") as in_file:
        for line in in_file:
            stripped = line.strip()
            if not stripped:
                continue
            left, right = stripped.split(",", 1)
            pairs.append((int(left), int(right)))
    return pairs


def do_add(conn: sqlite3.Connection, relation: str, relative_data_path: str) -> None:
    create_relation(conn, relation)
    pairs = read_pairs(DBSI_DIR / relative_data_path)
    rel = quote_ident(relation)
    conn.executemany(f"INSERT INTO {rel} (c1, c2) VALUES (?, ?)", pairs)


def do_delete(conn: sqlite3.Connection, relation: str, relative_data_path: str) -> None:
    pairs = read_pairs(DBSI_DIR / relative_data_path)
    rel = quote_ident(relation)
    conn.executemany(f"DELETE FROM {rel} WHERE c1 = ? AND c2 = ?", pairs)


def do_query(conn: sqlite3.Connection, query_text: str) -> int:
    sql, params = query_to_count_sql(query_text)
    row = conn.execute(sql, params).fetchone()
    if row is None:
        return 0
    return int(row[0])


def run_bench_file(conn: sqlite3.Connection, bench_name: str) -> None:
    bench_path = BENCH_IN_DIR / bench_name
    with bench_path.open("r", encoding="utf-8") as bench_file:
        for raw_line in bench_file:
            line = raw_line.strip()
            if not line:
                continue

            if line.startswith("BEGIN ") or line.startswith("COMMIT ") or line.startswith("ROLLBACK "):
                continue

            if line.startswith("ADD "):
                _, _tx, relation, data_path = line.split(maxsplit=3)
                do_add(conn, relation, data_path)
                continue

            if line.startswith("DELETE "):
                _, _tx, relation, data_path = line.split(maxsplit=3)
                do_delete(conn, relation, data_path)
                continue

            if line.startswith("QUERY "):
                _, _tx, query_text = line.split(maxsplit=2)
                count = do_query(conn, query_text)
                print(f"{bench_name}: {query_text}")
                print(f"  number_of_tuples={count}")
                continue

            raise ValueError(f"Unsupported command in {bench_name}: {line}")


def main() -> None:
    conn = sqlite3.connect(":memory:")
    conn.execute("PRAGMA journal_mode = OFF")
    conn.execute("PRAGMA synchronous = OFF")
    conn.execute("PRAGMA temp_store = MEMORY")
    conn.execute("PRAGMA cache_size = -200000")

    run_bench_file(conn, "bench_2.in")
    run_bench_file(conn, "bench_3.in")
    run_bench_file(conn, "bench_5.in")
    run_bench_file(conn, "bench_6.in")


if __name__ == "__main__":
    main()
