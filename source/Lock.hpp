#pragma once
#include "common.hpp"

class Lock {
public:
  virtual bool acquire(const TID &tid, LockMode mode) = 0;
  virtual void release(const TID &tid) = 0; // pre: tid holds the lock
  virtual ~Lock() = default;
};