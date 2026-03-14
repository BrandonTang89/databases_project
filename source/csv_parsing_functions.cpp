#include "csv_parsing_functions.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
#include <print>

ParseResult parse_csv_line(const std::string &line) {
  // Trim trailing CR so Windows line-endings (\r\n) are handled.
  std::string_view sv = line;
  if (!sv.empty() && sv.back() == '\r')
    sv.remove_suffix(1);
  if (sv.empty())
    return {false, 0, 0};

  auto comma = sv.find(',');
  if (comma == std::string_view::npos)
    return {false, 0, 0};

  std::string_view lhs = sv.substr(0, comma);
  std::string_view rhs = sv.substr(comma + 1);

  // Reject anything with a second comma.
  if (rhs.find(',') != std::string_view::npos)
    return {false, 0, 0};

  int left{}, right{};
  auto [lend, lerr] =
      std::from_chars(lhs.data(), lhs.data() + lhs.size(), left);
  if (lerr != std::errc{} || lend != lhs.data() + lhs.size())
    return {false, 0, 0};

  auto [rend, rerr] =
      std::from_chars(rhs.data(), rhs.data() + rhs.size(), right);
  if (rerr != std::errc{} || rend != rhs.data() + rhs.size())
    return {false, 0, 0};

  return {true, left, right};
}

std::vector<std::pair<int, int>> parse_csv_file(const std::string &filename) {
  std::vector<std::pair<int, int>> result;
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::println(std::cerr, "ERROR: cannot open file '{}'", filename);
    return result;
  }

  std::string line;
  int line_no = 0;
  while (std::getline(file, line)) {
    ++line_no;
    auto [ok, left, right] = parse_csv_line(line);
    if (!ok) {
      std::println(std::cerr, "WARNING: skipping malformed line {} in '{}': {}",
                   line_no, filename, line);
      continue;
    }
    result.emplace_back(left, right);
  }
  return result;
}