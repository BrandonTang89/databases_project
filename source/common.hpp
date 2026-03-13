#pragma once
#include <string>

using TID = std::string;
using RelName = std::string;
enum class StatusCode {
  SUCCESS,
  SUSPENDED,
};

enum class LockMode { SHARED, EXCLUSIVE };