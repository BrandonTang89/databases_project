#pragma once
#include "DataTuple.hpp"
#include "SLock.hpp"
#include <cassert>
#include <unordered_map>

struct Group {
  // DTI: all tuples are either (_, c) or (c, _) for some constant c
  // StableVector<DataTuple *, 8> tuples;
  // std::unordered_set<DataTuple *> tuples; // faster to search than a
  // StableVector for small groups, and we don't need order
  std::unordered_map<uint64_t, DataTuple *>
      tuples; // map from the non-constant value to the set of tuples with that
              // value, allows faster search for large groups
  SLock lock;

  inline uint64_t make_key(uint32_t left, uint32_t right) const {
    return (static_cast<uint64_t>(left) << 32) | right;
  }

  // Finds a tuple with the given left and right values, or returns nullptr if
  // no such tuple exists.
  DataTuple *find(uint32_t left, uint32_t right) {
    auto it = tuples.find(make_key(left, right));
    if (it != tuples.end()) {
      return it->second;
    } else {
      return nullptr;
    }
  }

  bool insert(DataTuple *tp) {
    // Precondition: tp is not already in tuples and matches the DTI pattern.
    assert(!find(tp->left, tp->right));
    tuples[make_key(tp->left, tp->right)] = tp;
    return true;
  }
};