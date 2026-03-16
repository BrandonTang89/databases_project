#pragma once
#include <string>
#include <vector>
#include <stdint.h>

// Returns true when an alive tuple (left, right) already exists in rel.
struct ParseResult {
  bool ok;
  uint32_t left;
  uint32_t right;
};

// Parse a single "a,b" CSV line.  Returns ok=false for any malformed input.
ParseResult parse_csv_line(const std::string &line);

/**
 * Parse an entire CSV file, printing warnings along the way
 * Returns a vector of (left, right) pairs for all well-formed lines.
 */
std::vector<std::pair<uint32_t, uint32_t>> parse_csv_file(const std::string &filename);