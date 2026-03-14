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

  bool add_tuple(TID tid, int left, int right) {
    DataTuple *tp = ensure_tuple(left, right);
    bool isLocked = tp->lock.acquire(tid, LockMode::EXCLUSIVE);
    if (isLocked) {
      tp->alive = true;
    } else {
      return false;
    }
    assert(false);
  }

  bool delete_tuple(TID tid, int left, int right) {
    DataTuple *tp = ensure_tuple(left, right);
    bool isLocked = tp->lock.acquire(tid, LockMode::EXCLUSIVE);
    if (isLocked) {
      tp->alive = false;
    } else {
      return false;
    }
    assert(false);
  }

private:
  DataTuple *ensure_tuple(int left, int right) {
    Group group = leftToRightIndex[left];
    DataTuple *tp = group.find(left, right);
    if (!tp) {
      // tuple does not exist, so we need to add it
      tp = tuples.emplace(left, right);
      leftToRightIndex[left].insert(tp);
      rightToLeftIndex[right].insert(tp);
      return tp;
    } else {
      return tp;
    }
  }
};