#pragma once
#include "XSLock.hpp"

struct DataTuple {
  int left{};
  int right{};
  bool alive{true};
  XSLock lock{};
};
