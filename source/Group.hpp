#pragma once
#include "DataTuple.hpp"
#include "SLock.hpp"
#include <cassert>
#include <flat_set>

struct Group {
  // DTI: all tuples are either (_, c) or (c, _) for some constant c
  std::flat_set<DataTuple *> tuples;
  SLock lock;

  DataTuple* find(int left, int right) {
    for (DataTuple *tp : tuples) {
      if (tp->alive && tp->left == left && tp->right == right)
        return tp;
    }
    return nullptr;
  }

  bool insert(DataTuple *tp) {
    // Precondition: tp is not already in tuples and matches the DTI pattern.
    assert(!find(tp->left, tp->right));
    return tuples.insert(tp).second;
  }
};