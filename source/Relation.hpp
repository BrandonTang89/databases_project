#pragma once
#include "DataTuple.hpp"
#include "Group.hpp"
#include "SLock.hpp"
#include "StableVector.hpp"
#include "common.hpp"
#include <unordered_map>

class Relation {

public:
  StableVector<DataTuple, 1> tuples;
  std::unordered_map<int, Group> leftToRightIndex;
  std::unordered_map<int, Group> rightToLeftIndex;
  SLock whole_rel_lock{};
  SLock diagonal_lock{};

  /**
   * returns true on success, false if the transaction cannot acquire the
   * necessary locks
   *
   * Doesn't check whole relation lock for efficiency, caller should check
   * before calling this method
   */
  bool edit_tuple(Transaction &tx, int left, int right, bool newAlive);

private:
  /**
   * Ensures that a tuple with the given left and right values exists in the
   * relation.
   * Once a tuple is created, it is never removed.
   * Always safe to do regardless of any locks
   */
  DataTuple *ensure_tuple(int left, int right);

  bool check_predicate_locks(const TID &tid, int left, int right);
};