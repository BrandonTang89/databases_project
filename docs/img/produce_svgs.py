#!/usr/bin/env python3

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    img_dir = Path(__file__).resolve().parent
    mermaid_files = sorted(img_dir.glob("*.mermaid"))

    if not mermaid_files:
        print(f"No .mermaid files found in {img_dir}")
        return 0

    missing_tools: list[str] = []
    if shutil.which("mmdc") is None:
        missing_tools.append("mmdc")
    if shutil.which("mutool") is None:
        missing_tools.append("mutool")

    if missing_tools:
        print(
            "ERROR: Missing required tool(s): " + ", ".join(missing_tools),
            file=sys.stderr,
        )
        return 1

    failures = 0

    for mermaid_file in mermaid_files:
        stem = mermaid_file.stem
        pdf_name = f"{stem}.pdf"
        svg_name = f"{stem}.svg"

        print(f"Generating {svg_name} from {mermaid_file.name}")

        try:
            subprocess.run(
                [
                    "mmdc",
                    "-i",
                    mermaid_file.name,
                    "-o",
                    pdf_name,
                    "--pdfFit",
                    "-b",
                    "transparent",
                    "-t",
                    "neutral",
                ],
                cwd=img_dir,
                check=True,
            )

            subprocess.run(
                ["mutool", "draw", "-o", svg_name, pdf_name],
                cwd=img_dir,
                check=True,
            )

        except subprocess.CalledProcessError as error:
            failures += 1
            print(
                f"ERROR: Failed to generate {svg_name} (exit code {error.returncode}).",
                file=sys.stderr,
            )

        finally:
            (img_dir / pdf_name).unlink(missing_ok=True)

    if failures:
        print(f"Completed with {failures} failure(s).", file=sys.stderr)
        return 1

    print(f"Done. Generated {len(mermaid_files)} SVG file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
