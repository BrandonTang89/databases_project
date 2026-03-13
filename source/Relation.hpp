#pragma once
#include "DataTuple.hpp"
#include "Group.hpp"
#include "SLock.hpp"
#include "StableVector.hpp"
#include <unordered_map>

class Relation {

public:
  StableVector<DataTuple, 1> tuples;
  std::unordered_map<int, Group> leftToRightIndex;
  std::unordered_map<int, Group> rightToLeftIndex;
  SLock whole_rel_lock{};
  SLock diagonal_lock{};
};