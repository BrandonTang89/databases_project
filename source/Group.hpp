#pragma once
#include "DataTuple.hpp"
#include "SLock.hpp"
#include <vector>

struct Group {
  std::vector<DataTuple *> tuples;
  SLock lock;
};