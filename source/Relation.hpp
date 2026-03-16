#pragma once
#include "DataTuple.hpp"
#include "Group.hpp"
#include "SLock.hpp"
#include "StableVector.hpp"
#include "common.hpp"
#include <unordered_map>

class Relation {

public:
  StableVector<DataTuple> tuples;
  std::unordered_map<int, Group> leftToRightIndex;
  std::unordered_map<int, Group> rightToLeftIndex;
  Group diagonalIndex; // index for tuples where left == right
  SLock whole_rel_lock{};

  /**
   * returns true on success, false if the transaction cannot acquire the
   * necessary locks
   *
   * Doesn't check whole relation lock for efficiency, caller should check
   * before calling this method
   */
  bool edit_tuple(Transaction &tx, int left, int right, bool newAlive);

  /** Ensures the tuple is created and thus always not null
   */
  DataTuple *get_tuple(int left, int right);

private:
  /**
   * Ensures that a tuple with the given left and right values exists in the
   * relation.
   * Once a tuple is created, it is never removed.
   * Always safe to do regardless of any locks
   */
  DataTuple *ensure_tuple(int left, int right);

  /**
   * Checks if the transaction is permitted to modify the tuple with respect to
   * the group locks.
   */
  bool check_group_locks(const TID &tid, int left, int right);

  /**
   * Adds the group locks that the transaction would need to pass to edit the
   * tuple
   */
  void dep_group_locks(Transaction &tx, int left, int right);
};