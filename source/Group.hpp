#pragma once
#include "DataTuple.hpp"
#include "SLock.hpp"
#include <vector>

struct Group {
  // DTI: all tuples are either (_, c) or (c, _) for some constant c
    std::vector<DataTuple *> tuples;
  SLock lock;

  
};