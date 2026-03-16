# Agent Instructions

This is a C++23 database mini-project implementing a concurrent transaction engine
with lock-based concurrency control and deadlock detection.

---

## Build Commands

The project uses CMake with Ninja and two presets. A Nix devshell (via direnv) provides
the toolchain. Always enter the devshell before building (`direnv allow` or `use flake`).

```bash
# Build debug (ASAN enabled, -g -O)
build debug
# Equivalent without the shell helper:
cmake --build --preset debug -j 8

# Build release (-O3 -DNDEBUG, ASAN off)
build release
cmake --build --preset release -j 8

# Binaries are placed at:
./build/debug/databases_project
./build/release/databases_project
```

The `build` shell helper is defined in `flake.nix`. Always use `build debug` before
running tests.

---

## Test Commands

Tests live in `tests/`. There are two test runners:

### Integration tests (run all at once, compare stdout)
```bash
# Run a single named test
build debug && python3 -m tests.run_tests question

# Run all tests
build debug && python3 -m tests.run_tests all
```

### Simulator (interactive, command-by-command with live output)
```bash
# Run a single named test interactively
build debug && python3 -m tests.simulator question
```

Test interaction files are in `tests/interactions/`. Each file contains lines starting
with `"> "` (commands to send) interleaved with expected output lines. The `run_tests`
runner supports the `<NUMBER>` token in expected output to match any integer.

CSV data files used by tests are in `tests/running_dir/`.

---

## Project Structure

```
source/         C++ source and headers (flat, no subdirectories)
tests/
  interactions/ Test interaction scripts
  running_dir/  CSV data files used by tests
  run_tests.py  Integration test runner
  simulator.py  Interactive simulator test runner
docs/           Design documents (PDF, Typst)
build/
  debug/        Debug build output
  release/      Release build output
```

Key source files:
- `source/common.hpp` — type aliases, enums, logging helpers, shared structs
- `source/Database.{hpp,cpp}` — top-level coordinator; owns transactions and relations
- `source/Transaction.{hpp,cpp}` — transaction state machine; handles suspend/resume
- `source/Relation.{hpp,cpp}` — relation storage; manages tuple-level locking
- `source/Stage.{hpp,cpp}` — pipelined query execution engine
- `source/DeadlockDetector.{hpp,cpp}` — DFS-based cycle detection over waits-for graph
- `source/Lock.hpp` — abstract `Lock` interface
- `source/SLock.hpp` — shared-only lock (relation/group level)
- `source/XSLock.hpp` — exclusive/shared lock (tuple level)
- `source/StableVector.hpp` — stable-pointer container (pointers never invalidated)

---

## C++ Style Guidelines

### Standard and Compiler
- **C++23** throughout (`std::println`, `std::flat_set`, `std::flat_map`, `std::print`,
  `std::format_string`, `std::optional`, `std::variant`).
- Compiled with GCC 15. All warnings enabled: `-Wall -Wextra -Wpedantic -Wshadow
  -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused -Woverloaded-virtual
  -Wconversion -Wsign-conversion -Wdouble-promotion -Wformat=2`. Code must compile
  cleanly with zero warnings.
- ASAN (`-fsanitize=address`) is on in debug builds. Do not introduce memory errors.

### Naming Conventions
| Construct | Convention | Example |
|---|---|---|
| Classes | `PascalCase` | `DeadlockDetector`, `StableVector` |
| Structs | `PascalCase` | `DataTuple`, `QueryAtom`, `ParseResult` |
| Enums | `enum class`, PascalCase type | `enum class LockMode` |
| Enum members | `SCREAMING_SNAKE_CASE` | `LockMode::SHARED`, `TransactionState::EXECUTING_ADD` |
| Type aliases | `PascalCase` | `TID`, `RelName` |
| Member variables (public/protected) | `snake_case` | `startTime`, `query_atoms` |
| Member variables (private, in lock classes) | `snake_case` with trailing `_` | `lock_holders_`, `held_exclusively_` |
| Methods | `snake_case` | `edit_tuple`, `dep_group_locks`, `current_holders` |
| Free functions | `snake_case` | `tokenise`, `parse_csv_line` |
| Parameters and locals | `snake_case` | `tid`, `csv_file`, `new_alive` |

### Headers
- Use `#pragma once` (not include guards).
- All headers are in `source/` with no subdirectories.
- Forward-declare in `common.hpp` where needed (`class Database;` etc.).
- Include only what is needed; prefer forward declarations in headers.

### Imports / Includes
- Standard library headers use `<angle brackets>`.
- Project headers use `"double_quotes"`.
- Group: standard library first, then project headers.

### Output and Logging
- All user-visible output goes through an injected `std::ostream &out` member, using
  `std::println(out, "...", args...)`.
- Error messages are always prefixed exactly `"ERROR: "`.
- Debug logging uses `debug("...", args...)` (prints to stderr with `[DEBUG]` prefix).
- Trace logging uses `trace(...)` (compile-time disabled via `constexpr bool enable_trace`).
- Do not use `std::cout` directly except in `resume_query` where query answer rows are
  printed (existing pattern); prefer `out` for everything else.

### Error Handling
- **Parse/lookup failures**: return `bool` or `std::optional<T>`.
- **Transaction operation results**: return `StatusCode` (`SUCCESS` or `SUSPENDED`).
- **Precondition violations**: guard with state checks at the top of methods, print
  `"ERROR: ..."` to `out`, and return `StatusCode::SUCCESS` (not `SUSPENDED`).
- Do not throw exceptions. Do not use `abort()` except via `assert()` for unreachable
  code paths.
- Use `assert(false)` to mark logically unreachable branches.

### Class Design
- Use `friend class` declarations sparingly (currently: `Relation`, `Stage`,
  `DeadlockDetector` are friends of `Transaction` to access private lock state).
- Prefer injecting dependencies (e.g. `std::ostream &out`, `std::unordered_map<TID,
  Transaction> &transactions`) via constructor parameters rather than globals.
- Non-copyable types should delete the copy constructor or hold references.

### Concurrency / Locking Invariants
- `Transaction::acquire(lock, mode)` is the **only** correct way to attempt lock
  acquisition. It inserts into `held_locks` on success and `required_locks` on failure.
- Insert into `required_locks` **only when a lock acquisition actually fails**, never
  pre-emptively before checking availability. This is critical for correct deadlock
  detection (a spurious entry causes false cycles).
- `required_locks` is cleared at the start of `Transaction::resume()` before retrying.
- `std::unordered_map::operator[]` must not be used on maps whose value type lacks a
  default constructor (e.g. `transactions[tid]`). Use `.at(tid)` instead.

### Modern C++ Patterns
- Prefer structured bindings (`auto &[left, right] = ...`).
- Use `std::variant` + `overloaded` visitor for `QueryArg` dispatch.
- Use `std::flat_set` / `std::flat_map` for small ordered sets/maps with contiguous
  storage; use `std::unordered_map` / `std::unordered_set` for larger hash maps.
- Use `std::optional<T>` for nullable return values instead of sentinel values or
  output parameters.
- Use `std::string_view` for read-only string parameters where possible.

---

## Architecture Notes

- **Suspend/Resume model**: operations that cannot acquire a lock return
  `StatusCode::SUSPENDED`. `Database::on_control` handles this by running deadlock
  detection and either resolving the deadlock or leaving the transaction suspended
  until `RESUME` is issued externally.
- **Deadlock detection**: triggered on every suspension. Runs a DFS from the newly
  suspended transaction over the waits-for graph (`required_locks` → `current_holders()`).
  Youngest victim (highest `startTime`) is selected and rolled back.
- **Query pipeline**: `Stage` objects implement a pull-based iterator pipeline. Each
  stage calls `previous->next()` to get input tuples, filters/joins, and returns
  `PipelineStatus::OK` (result ready), `SUSPEND` (blocked on a lock), or `FINISHED`.
- **Stable pointers**: `StableVector` is used wherever long-lived pointers to elements
  are needed (e.g. `DataTuple*` stored in `Group`, lock pointers in `required_locks`).
  Never use `std::vector` for such elements — reallocation would invalidate pointers.
