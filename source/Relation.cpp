#include "Relation.hpp"
#include "DataTuple.hpp"
#include "Group.hpp"
#include "SLock.hpp"
#include "StableVector.hpp"
#include "Transaction.hpp"
#include <unordered_map>

bool Relation::check_group_locks(const TID &tid, uint32_t left, uint32_t right) {
  if (left == right) {
    if (!diagonalIndex.lock.permits(tid)) {
      debug("Transaction {} is waiting for diagonal_lock", tid);
      return false;
    }
  }

  Group &leftGroup = leftToRightIndex[left];
  Group &rightGroup = rightToLeftIndex[right];
  if (!leftGroup.lock.permits(tid)) {
    debug("Transaction {} is waiting for left group lock for value {}", tid,
          left);
    return false;
  }
  if (!rightGroup.lock.permits(tid)) {
    debug("Transaction {} is waiting for right group lock for value {}", tid,
          right);
    return false;
  }
  return true;
}

void Relation::dep_group_locks(Transaction &tx, uint32_t left, uint32_t right) {
  if (left == right) {
    tx.required_locks.insert(&diagonalIndex.lock);
  }
  Group &leftGroup = leftToRightIndex[left];
  Group &rightGroup = rightToLeftIndex[right];
  tx.required_locks.insert(&leftGroup.lock);
  tx.required_locks.insert(&rightGroup.lock);
}

bool Relation::edit_tuple(Transaction &tx, uint32_t left, uint32_t right, bool newAlive) {
  // Only called by adding query
  const TID &tid = tx.tid;
  DataTuple *tp = ensure_tuple(left, right);
  if (check_group_locks(tid, left, right) &&
      tx.acquire(tp->lock, LockMode::EXCLUSIVE)) {
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
    return true;

  } else {
    tx.required_locks.insert(&tp->lock); 
    tx.required_locks.insert(&whole_rel_lock);
    dep_group_locks(tx, left, right);

    debug("Transaction {} is waiting to acquire lock for tuple ({}, {})", tid,
          left, right);
    return false;
  }
  assert(false);
}

DataTuple *Relation::get_tuple(uint32_t left, uint32_t right) {
  return ensure_tuple(left, right);
}

DataTuple *Relation::ensure_tuple(uint32_t left, uint32_t right) {
  Group &group = leftToRightIndex[left];
  DataTuple *tp = group.find(left, right);
  if (!tp) {
    // tuple does not exist, so we need to add it
    tp = tuples.emplace(left, right);
    leftToRightIndex[left].insert(tp);
    rightToLeftIndex[right].insert(tp);
    if (left == right) {
      diagonalIndex.insert(tp);
    }
    return tp;
  } else {
    return tp;
  }
}