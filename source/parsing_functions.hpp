#pragma once
#include "common.hpp"
#include <optional>
#include <vector>

// Tokenise a single input line into whitespace-separated tokens.
std::vector<std::string> tokenise(const std::string &line);

// Parse a single argument token: an integer constant or a bare identifier.
std::optional<QueryArg> parse_term(std::string_view sv);

// Parse a single atom of the form  RelName(arg, arg)
// Returns nullopt and sets *err_msg on failure.
std::optional<QueryAtom> parse_atom(std::string_view sv, std::string &err_msg);

// Split the query body into atom tokens, respecting parentheses so that the
// comma inside R(X,Y) does not break the split.
// Then parse each token as a QueryAtom.
// Returns nullopt and sets *err_msg on the first parse failure.
std::optional<std::vector<QueryAtom>> parse_query(const std::string &body,
                                                  std::string &err_msg);