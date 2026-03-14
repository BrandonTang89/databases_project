#!/usr/bin/env python3
"""
simulator.py  <interaction-name>

Runs the debug binary in a pseudo-terminal so that the binary's isatty()
check passes and it emits the "> " prompt.  Commands are fed one at a time,
each after the prompt appears, reproducing what a human session looks like.

Both stdout and stderr are printed as they arrive; no output checking is done.
"""

import argparse
import os
import pty
import select
import sys
import termios
from pathlib import Path

TESTS_DIR   = Path(__file__).resolve().parent
WORKSPACE   = TESTS_DIR.parent
BINARY      = WORKSPACE / "build" / "debug" / "databases_project"
RUNNING_DIR = TESTS_DIR / "running_dir"
INTERACTIONS = TESTS_DIR / "interactions"

PROMPT = b"> "


def parse_inputs(path: Path) -> list[str]:
    """Extract only the input lines (those starting with '> ') from an interaction file."""
    inputs: list[str] = []
    for raw in path.read_text().splitlines():
        if raw.startswith("> "):
            inputs.append(raw[2:])
    return inputs


def run_simulation(name: str) -> None:
    interaction_path = INTERACTIONS / name
    if not interaction_path.exists():
        print(f"ERROR: interaction file not found: {interaction_path}", file=sys.stderr)
        sys.exit(1)

    if not BINARY.exists():
        print(f"ERROR: binary not found: {BINARY}", file=sys.stderr)
        print("       Build the project first (cmake --build build).", file=sys.stderr)
        sys.exit(1)

    inputs = parse_inputs(interaction_path)

    # Open a PTY pair.  master_fd is our end; the child gets slave_fd as its
    # stdin/stdout so isatty() returns True inside the binary.
    master_fd, slave_fd = pty.openpty()

    # Disable echo on the master side so we don't see every sent byte twice.
    attrs = termios.tcgetattr(master_fd)
    attrs[3] &= ~termios.ECHO   # lflags: clear ECHO
    termios.tcsetattr(master_fd, termios.TCSANOW, attrs)

    # Also open a separate PTY pair for stderr so we capture it too.
    err_master, err_slave = pty.openpty()

    pid = os.fork()
    if pid == 0:
        # --- child ---
        os.close(master_fd)
        os.close(err_master)
        os.dup2(slave_fd, 0)   # stdin
        os.dup2(slave_fd, 1)   # stdout
        os.dup2(err_slave, 2)  # stderr
        os.close(slave_fd)
        os.close(err_slave)
        os.chdir(str(RUNNING_DIR))
        os.execv(str(BINARY), [str(BINARY)])
        os._exit(1)  # execv failed

    # --- parent ---
    os.close(slave_fd)
    os.close(err_slave)

    buf = b""          # rolling read buffer for stdout/PTY data
    cmd_iter = iter(inputs)
    next_cmd: str | None = next(cmd_iter, None)
    done = False

    try:
        while True:
            fds = [master_fd, err_master]
            readable, _, _ = select.select(fds, [], [], 0.5)

            if not readable:
                # Timeout – check if child has exited
                result = os.waitpid(pid, os.WNOHANG)
                if result[0] != 0:
                    done = True
                    break
                continue

            for fd in readable:
                try:
                    data = os.read(fd, 4096)
                except OSError:
                    done = True
                    break

                if not data:
                    done = True
                    break

                if fd == master_fd:
                    # Print stdout data immediately (binary→text, best-effort)
                    sys.stdout.write(data.decode(errors="replace"))
                    sys.stdout.flush()
                    buf += data

                    # After we see the prompt, send the next command.
                    if next_cmd is not None and buf.endswith(PROMPT):
                        buf = b""
                        line = (next_cmd + "\n").encode()
                        # Print the command ourselves (echo is disabled on the PTY).
                        sys.stdout.write(next_cmd + "\n")
                        sys.stdout.flush()
                        os.write(master_fd, line)
                        next_cmd = next(cmd_iter, None)

                else:  # stderr fd
                    sys.stderr.write(data.decode(errors="replace"))
                    sys.stderr.flush()

            if done:
                break

            # If we've exhausted commands and the child is still running,
            # send Ctrl-D (EOF) to let it exit gracefully.
            if next_cmd is None and not done:
                # Only send once; flip flag via a sentinel
                next_cmd = ""   # empty string means "already sent EOF"
                try:
                    # Send EOT character to close stdin
                    os.write(master_fd, b"\x04")
                except OSError:
                    pass

    finally:
        # Drain any remaining output
        for fd in (master_fd, err_master):
            try:
                while True:
                    r, _, _ = select.select([fd], [], [], 0.1)
                    if not r:
                        break
                    data = os.read(fd, 4096)
                    if not data:
                        break
                    if fd == master_fd:
                        sys.stdout.write(data.decode(errors="replace"))
                        sys.stdout.flush()
                    else:
                        sys.stderr.write(data.decode(errors="replace"))
                        sys.stderr.flush()
            except OSError:
                pass

        os.close(master_fd)
        os.close(err_master)

        # Reap child
        try:
            os.waitpid(pid, 0)
        except ChildProcessError:
            pass


def main() -> None:
    parser = argparse.ArgumentParser(description="Simulate interactive use of the database binary.")
    parser.add_argument("name", help="Name of the interaction file in tests/interactions/")
    args = parser.parse_args()
    run_simulation(args.name)


if __name__ == "__main__":
    main()
