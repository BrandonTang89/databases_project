#pragma once
#include "DataTuple.hpp"
#include "Group.hpp"
#include "SLock.hpp"
#include "StableVector.hpp"
#include "common.hpp"
#include <unordered_map>

class Relation {

public:
  using TupleContainer = StableVector<DataTuple, 2048>;
  std::unordered_map<uint32_t, Group> l_to_r_index;
  std::unordered_map<uint32_t, Group> r_to_l_index;
  Group diagonal_index; // index for tuples where left == right
  SLock whole_rel_lock{};

  /**
   * returns true on success, false if the transaction cannot acquire the
   * necessary locks
   *
   * Doesn't check whole relation lock for efficiency, caller should check
   * before calling this method
   */
  bool edit_tuple(Transaction &tx, uint32_t left, uint32_t right,
                  bool newAlive);

  /** Ensures the tuple is created and thus always not null
   */
  DataTuple *get_tuple(uint32_t left, uint32_t right);

  TupleContainer::iterator begin() { return tuples.begin(); }
  TupleContainer::iterator end() { return tuples.end(); }

private:
  TupleContainer tuples;
  /**
   * Ensures that a tuple with the given left and right values exists in the
   * relation.
   * Once a tuple is created, it is never removed.
   * Always safe to do regardless of any locks
   */
  DataTuple *ensure_tuple(uint32_t left, uint32_t right);

  /**
   * Checks if the transaction is permitted to modify the tuple with respect to
   * the group locks.
   */
  bool check_group_locks(const TID &tid, uint32_t left, uint32_t right,
                         Group &left_group, Group &right_group);

  /**
   * Adds the group locks that the transaction would need to pass to edit the
   * tuple
   */
  void dep_group_locks(Transaction &tx, uint32_t left, uint32_t right,
                       Group &left_group, Group &right_group);
};
