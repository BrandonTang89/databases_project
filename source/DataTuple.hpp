#pragma once
#include "XSLock.hpp"

struct DataTuple {
  uint32_t left{};
  uint32_t right{};
  bool alive{false};
  XSLock lock{};
};
