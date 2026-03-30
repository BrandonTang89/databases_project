#include "Relation.hpp"
#include "DataTuple.hpp"
#include "Group.hpp"
#include "SLock.hpp"
#include "StableVector.hpp"
#include "Transaction.hpp"
#include <unordered_map>

bool Relation::check_group_locks(const TID &tid, uint32_t left, uint32_t right,
                                 Group &left_group, Group &right_group) {
  if (left == right) {
    if (!diagonal_index.lock.permits_edit(tid)) {
      debug("Transaction {} is waiting for diagonal_lock", tid);
      return false;
    }
  }

  if (!left_group.lock.permits_edit(tid)) {
    debug("Transaction {} is waiting for left group lock for value {}", tid,
          left);
    return false;
  }
  if (!right_group.lock.permits_edit(tid)) {
    debug("Transaction {} is waiting for right group lock for value {}", tid,
          right);
    return false;
  }
  return true;
}

void Relation::dep_group_locks(Transaction &tx, uint32_t left, uint32_t right,
                               Group &left_group, Group &right_group) {
  if (left == right) {
    tx.required_locks.insert(&diagonal_index.lock);
  }
  tx.required_locks.insert(&left_group.lock);
  tx.required_locks.insert(&right_group.lock);
}

bool Relation::edit_tuple(Transaction &tx, uint32_t left, uint32_t right,
                          bool newAlive) {
  // Only called by adding query
  const TID &tid = tx.tid;
  Group &left_group = l_to_r_index[left];
  Group &right_group = r_to_l_index[right];
  if (!check_group_locks(tid, left, right, left_group, right_group)) {
    tx.required_locks.insert(&whole_rel_lock);
    dep_group_locks(tx, left, right, left_group, right_group);
    debug("Transaction {} is waiting to acquire lock for tuple ({}, {})", tid,
          left, right);
    return false;
  }

  DataTuple *tp = ensure_tuple(left, right);
  if (tx.acquire(tp->lock, LockMode::EXCLUSIVE)) {
    // Treat all the lock parts as "atomic"
    // this ensures proper deadlock detection!

    if (tp->alive == newAlive) {
      // tuple already correct
      return true;
    } else {
      tx.store_original(tp);
      tx.num_modified++;
      tp->alive = newAlive;
      return true;
    }
  } else {
    // tuple lock already inserted via tx.acquire()
    tx.required_locks.insert(&whole_rel_lock);
    dep_group_locks(tx, left, right, left_group, right_group);
    debug("Transaction {} is waiting to acquire lock for tuple ({}, {})", tid,
          left, right);
    return false;
  }
}

DataTuple *Relation::get_tuple(uint32_t left, uint32_t right) {
  return ensure_tuple(left, right);
}

DataTuple *Relation::ensure_tuple(uint32_t left, uint32_t right) {
  Group &group = l_to_r_index[left];
  DataTuple *tp = group.find(left, right);
  if (!tp) {
    // tuple does not exist, so we need to add it
    tp = tuples.emplace(left, right);
    group.insert(tp);
    r_to_l_index[right].insert(tp);
    if (left == right) {
      diagonal_index.insert(tp);
    }
    return tp;
  } else {
    return tp;
  }
}
