#pragma once
#include <string>
#include <variant>

using TID = std::string;
using RelName = std::string;
enum class StatusCode {
  SUCCESS,
  SUSPENDED,
};

enum class LockMode { SHARED, EXCLUSIVE };

// Query Representation
struct Constant {
  int value;
};
struct Variable {
  std::string name;
};
using QueryArg = std::variant<Constant, Variable>;

struct QueryAtom {
  std::string relation;
  QueryArg left;
  QueryArg right;
};