#include "Relation.hpp"
#include "DataTuple.hpp"
#include "Group.hpp"
#include "SLock.hpp"
#include "StableVector.hpp"
#include "Transaction.hpp"
#include <unordered_map>

bool Relation::check_predicate_locks(const TID &tid, int left, int right) {
  if (left == right) {
    if (!diagonal_lock.permits(tid)) {
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

bool Relation::edit_tuple(Transaction &tx, int left, int right, bool newAlive) {
  // Only called by adding query
  const TID &tid = tx.tid;
  DataTuple *tp = ensure_tuple(left, right);
  if (!check_predicate_locks(tid, left, right)) {
    return false;
  }

  bool isLocked = tp->lock.acquire(tid, LockMode::EXCLUSIVE);
  if (isLocked) {
    tx.held_locks.insert(&tp->lock);
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
    debug("Transaction {} is waiting to acquire lock for tuple ({}, {})", tid,
          left, right);
    return false;
  }
  assert(false);
}

DataTuple *Relation::ensure_tuple(int left, int right) {
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