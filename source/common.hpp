#pragma once
#include <iostream>
#include <print>
#include <string>
#include <variant>

constexpr bool enable_trace = false;

class Database;
class Relation;
class Transaction;

using TID = std::string;
using RelName = std::string;
enum class StatusCode {
  SUCCESS,
  SUSPENDED,
};

enum class LockMode { SHARED, EXCLUSIVE };
enum class TransactionState {
  READY,
  EXECUTING_QUERY,
  EXECUTING_ADD,
  EXECUTING_DELETE,
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
void debug(std::format_string<Args...> fmt, Args &&...args) {
  std::cerr << "[DEBUG] ";
  std::println(std::cerr, fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void trace(std::format_string<Args...> fmt, Args &&...args) {
  if constexpr (enable_trace) {
    std::cerr << "[TRACE] ";
    std::println(std::cerr, fmt, std::forward<Args>(args)...);
  }
}

template <typename... Args>
void todo(std::format_string<Args...> fmt, Args &&...args) {
  std::cerr << "[TODO] ";
  std::println(std::cerr, fmt, std::forward<Args>(args)...);
}