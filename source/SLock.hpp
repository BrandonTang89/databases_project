#pragma once
#include "Lock.hpp"
#include "common.hpp"
#include <cassert>
#include <flat_set>

class SLock : public Lock {

private:
  std::flat_set<TID> lock_holders_; // likely to be small

public:
  SLock() = default;
  virtual ~SLock() override = default;

  virtual bool acquire(const TID &tid, [[maybe_unused]] LockMode mode) override {
    assert(mode == LockMode::SHARED);
    // Add tid to lock_holders_ if it's not already present.
    if (!is_held_by(tid)) {
      lock_holders_.insert(tid);
    }
    return true;
  }

  virtual void release(const TID &tid) override {
    // Remove tid from lock_holders_.
    assert(is_held_by(tid));
    lock_holders_.erase(tid);
  }

  bool is_held_by(const TID &tid) const {
    return lock_holders_.find(tid) != lock_holders_.end();
  }

  /**
   * A lock permits edits if there is no other lock holder on it
   */
  bool permits_edit(const TID &tid) const {
    return lock_holders_.empty() ||
           (lock_holders_.size() == 1 && is_held_by(tid));
  }

  const std::flat_set<TID> &current_holders() const override {
    return lock_holders_;
  }
  
  size_t holder_count() const { return lock_holders_.size(); }
  bool is_held_exclusively() const override {
    return false; // SLock is never held exclusively
  }
};