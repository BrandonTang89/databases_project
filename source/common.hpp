#pragma once
#include <cstdint>
#include <iostream>
#include <print>
#include <string>
#include <variant>

constexpr bool enable_trace = false;

#ifndef NDEBUG
constexpr bool enable_debug = true;
#else
constexpr bool enable_debug = false;
#endif

class Database;
class Relation;
class Transaction;

using TID = std::string;
using RelName = std::string;
enum class StatusCode : uint8_t {
  FINISHED,
  SUSPENDED,
};

enum class LockMode : uint8_t { SHARED, EXCLUSIVE };
enum class TransactionState : uint8_t {
  READY,
  EXECUTING_QUERY,
  EXECUTING_ADD,
  EXECUTING_DELETE,
  COMMITTED,
  ABORTED
};

// Query Representation
struct Constant {
  uint32_t value;
};
struct Variable {
  std::string name;
};
using QueryArg = std::variant<Constant, Variable>;

struct QueryAtom {
  std::string relation;
  QueryArg left;
  QueryArg right;

  std::string to_string() const {
    auto arg_to_string = [](const QueryArg &arg) -> std::string {
      if (std::holds_alternative<Constant>(arg)) {
        return std::to_string(std::get<Constant>(arg).value);
      } else {
        return std::get<Variable>(arg).name;
      }
    };
    return std::format("{}({}, {})", relation, arg_to_string(left),
                       arg_to_string(right));
  }
};

enum class PipelineStatus : uint8_t {
  OK,
  SUSPEND,
  FINISHED,
};

template <typename... Args>
void debug(std::format_string<Args...> fmt, Args &&...args) {
  if constexpr (enable_debug) {
    std::cerr << "[DEBUG] ";
    std::println(std::cerr, fmt, std::forward<Args>(args)...);
  }
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

// for visiting variants
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
