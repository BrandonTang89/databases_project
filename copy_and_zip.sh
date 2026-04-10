#!/bin/bash
set -euo pipefail

# Directory where this script lives
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST_DIR="$SRC_DIR/../DatabasesSource"
ZIP_FILE="$SRC_DIR/../1075201.zip"

# Clean up any previous run
rm -rf "$DEST_DIR" "$ZIP_FILE"

# Create destination directory
mkdir -p "$DEST_DIR"

rsync -a \
  --exclude='build/' \
  --exclude='.mypy_cache/' \
  --exclude='.cache/' \
  --exclude='.direnv/' \
  --exclude='__pycache__/' \
  --exclude='benchmarks/benchmark_parts/bench_out/profiles/' \
  --exclude='.git/' \
  --exclude='AGENTS.md' \
  --exclude='docs/14-Transactions-and-Concurrency-1pp.pdf' \
  --exclude='docs/TakeHomeQuestions.pdf' \
  --exclude='copy_and_zip.sh' \
  "$SRC_DIR/" "$DEST_DIR/"

# Zip the result (placed next to DatabasesSource/)
zip -r "$ZIP_FILE" "$DEST_DIR/"

echo "Done! Created $ZIP_FILE"
