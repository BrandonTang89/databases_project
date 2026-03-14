#pragma once
#include <string>
#include <vector>

// Returns true when an alive tuple (left, right) already exists in rel.
struct ParseResult {
  bool ok;
  int left;
  int right;
};

// Parse a single "a,b" CSV line.  Returns ok=false for any malformed input.
ParseResult parse_csv_line(const std::string &line);

/**
 * Parse an entire CSV file, printing warnings along the way
 * Returns a vector of (left, right) pairs for all well-formed lines.
 */
std::vector<std::pair<int, int>> parse_csv_file(const std::string &filename);