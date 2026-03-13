#pragma once
#include "Lock.hpp"
#include "common.hpp"
#include <cassert>
#include <flat_set>

class XSLock : public Lock {

private:
  std::flat_set<TID> lock_holders_; // likely to be small
  bool held_exclusively_ = false;
  // held_exclusively => lock_holders_.size() == 1

public:
  XSLock() = default;

  virtual bool acquire(const TID &tid, LockMode mode) override {
    if (mode == LockMode::EXCLUSIVE) {
      if (holder_count() >= 2) {
        // Exclusive mode acquisition fails if the lock is held by multiple
        // transactions.
        return false;
      } else if (holder_count() == 1 && !is_held_by(tid)) {
        // Exclusive mode acquisition fails if the lock is held by another
        // transaction.
        return false;
      } else {
        lock_holders_.insert(tid);
        // possibly upgrade
        held_exclusively_ = true;
        return true;
      }
    } else {
      if (held_exclusively_) {
        return is_held_by(tid);
      } else {
        lock_holders_.insert(tid);
        return true;
      }
    }
    assert(false); // unreachable
}

  virtual void release(const TID &tid) override {
    // Remove tid from lock_holders_.
    assert(is_held_by(tid));
    lock_holders_.erase(tid);
    if (held_exclusively_) {
        assert(holder_count() == 0);
        held_exclusively_ = false;
    }
  }

  bool is_held_by(const TID &tid) const {
    return lock_holders_.find(tid) != lock_holders_.end();
  }

  size_t holder_count() const { return lock_holders_.size(); }

  virtual ~XSLock() override = default;
};