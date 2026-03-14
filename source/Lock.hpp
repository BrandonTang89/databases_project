#pragma once
#include "common.hpp"
#include <flat_set>

class Lock {
public:
  virtual bool acquire(const TID &tid, LockMode mode) = 0;
  virtual void release(const TID &tid) = 0; // pre: tid holds the lock
  virtual bool is_held_exclusively() const = 0;
  virtual const std::flat_set<TID>& current_holders() const = 0;
  virtual ~Lock() = default;
};