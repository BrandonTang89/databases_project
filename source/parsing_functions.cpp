#include "parsing_functions.hpp"
#include "common.hpp"
#include <charconv>
#include <optional>
#include <sstream>
#include <vector>

std::vector<std::string> tokenise(const std::string &line) {
  std::vector<std::string> tokens;
  std::istringstream iss(line);
  std::string tok;
  while (iss >> tok)
    tokens.push_back(std::move(tok));
  return tokens;
}

std::optional<QueryArg> parse_term(std::string_view sv) {
  // Trim surrounding whitespace.
  while (!sv.empty() && sv.front() == ' ')
    sv.remove_prefix(1);
  while (!sv.empty() && sv.back() == ' ')
    sv.remove_suffix(1);
  if (sv.empty())
    return std::nullopt;

  // Try integer first.
  uint32_t val{};
  auto [end, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
  if (ec == std::errc{} && end == sv.data() + sv.size())
    return Constant{val};

  // Must be an identifier: letters, digits, underscores.
  for (char c : sv)
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
      return std::nullopt;

  return Variable{std::string(sv)};
}

std::optional<QueryAtom> parse_atom(std::string_view sv,
                                           std::string &err_msg) {
  // Trim surrounding whitespace.
  while (!sv.empty() && sv.front() == ' ')
    sv.remove_prefix(1);
  while (!sv.empty() && sv.back() == ' ')
    sv.remove_suffix(1);

  auto lparen = sv.find('(');
  if (lparen == std::string_view::npos || sv.back() != ')') {
    err_msg = "expected 'RelName(arg, arg)', got: " + std::string(sv);
    return std::nullopt;
  }

  std::string_view rel_sv = sv.substr(0, lparen);
  std::string_view args_sv = sv.substr(lparen + 1, sv.size() - lparen - 2);

  // Relation name must be a non-empty identifier.
  while (!rel_sv.empty() && rel_sv.back() == ' ')
    rel_sv.remove_suffix(1);
  if (rel_sv.empty()) {
    err_msg = "empty relation name";
    return std::nullopt;
  }
  for (char c : rel_sv)
    if (!std::isalnum(static_cast<unsigned char>(c))) {
      err_msg = "invalid relation name: " + std::string(rel_sv);
      return std::nullopt;
    }

  // Split args on the single comma inside the parentheses.
  auto comma = args_sv.find(',');
  if (comma == std::string_view::npos) {
    err_msg = "expected two arguments in: " + std::string(sv);
    return std::nullopt;
  }

  auto larg = parse_term(args_sv.substr(0, comma));
  auto rarg = parse_term(args_sv.substr(comma + 1));

  if (!larg || !rarg) {
    err_msg = "invalid argument in: " + std::string(sv);
    return std::nullopt;
  }

  return QueryAtom{std::string(rel_sv), std::move(*larg), std::move(*rarg)};
}

std::optional<std::vector<QueryAtom>>
parse_query(const std::string &body, std::string &err_msg) {
  std::vector<QueryAtom> atoms;
  int depth = 0;
  std::size_t start = 0;

  for (std::size_t i = 0; i <= body.size(); ++i) {
    char c = (i < body.size()) ? body[i] : '\0';
    if (depth < 0 || depth > 1){
        err_msg = "query string brackets too nested or not well formed";
        return std::nullopt;
    }

    if (c == '(')
      ++depth;
    else if (c == ')')
      --depth;
    else if ((c == ',' && depth == 0) || c == '\0') {
      std::string_view token(body.data() + start, i - start);
      // Skip blank tokens (trailing comma etc.)
      bool blank = true;
      for (char ch : token)
        if (ch != ' ' && ch != '\t') {
          blank = false;
          break;
        }
      if (!blank) {
        auto atom = parse_atom(token, err_msg);
        if (!atom)
          return std::nullopt;
        atoms.push_back(std::move(*atom));
      }
      start = i + 1;
    }
  }

  if (atoms.empty()) {
    err_msg = "query body is empty";
    return std::nullopt;
  }
  return atoms;
}