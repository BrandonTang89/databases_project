#!/bin/bash
set -euo pipefail

# Directory where this script lives
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST_DIR="$SRC_DIR/../DatabasesSource"
ZIP_FILE="$SRC_DIR/../DatabasesSource.zip"

# Clean up any previous run
rm -rf "$DEST_DIR" "$ZIP_FILE"

# Create destination directory
mkdir -p "$DEST_DIR"

# Copy everything, excluding gitignored items + explicitly requested exclusions
rsync -a \
  --exclude='build/' \
  --exclude='.cache/' \
  --exclude='.direnv/' \
  --exclude='__pycache__/' \
  --exclude='benchmarks/benchmark_parts/bench_out/profiles/' \
  --exclude='out.pdf' \
  --exclude='.git/' \
  --exclude='AGENTS.md' \
  --exclude='docs/14-Transactions-and-Concurrency-1pp.pdf' \
  "$SRC_DIR/" "$DEST_DIR/"

# Zip the result (placed next to DatabasesSource/)
zip -r "$SRC_DIR/../DatabasesSource.zip" "$SRC_DIR/../DatabasesSource/"

echo "Done! Created $ZIP_FILE"
