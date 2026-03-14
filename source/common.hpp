#pragma once
#include <iostream>
#include <print>
#include <string>
#include <variant>

class Database;

using TID = std::string;
using RelName = std::string;
enum class StatusCode {
  SUCCESS,
  SUSPENDED,
};

enum class LockMode { SHARED, EXCLUSIVE };
enum class TransactionState {
  READY,
  SUSPENDED_QUERY,
  SUSPENDED_ADD,
  SUSPENDED_DELETE,
  COMMITTED,
  ABORTED
};


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

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args&&... args) {
  std::cerr << "[DEBUG] ";
  std::println(std::cerr, fmt, std::forward<Args>(args)...);
}