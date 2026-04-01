#pragma once

#include "hashtable_utils.hpp"
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_set>
#include <vector>

template <typename T>
concept Has64BitPointer = (sizeof(T *) == sizeof(uint64_t));

template <Has64BitPointer T> class PointerHashSetImpl {
  static constexpr T *EMPTY = nullptr;
  static inline T *const TOMBSTONE = reinterpret_cast<T *>(~uintptr_t{0});

  std::vector<T *> buckets;
  size_t size_{0};
  size_t tombstones_{0};

  size_t index_for(T *value) const {
    uint64_t bits = std::bit_cast<uint64_t>(value);
    return static_cast<size_t>(
        splitmix64(bits) & static_cast<uint64_t>(buckets.size() - 1));
  }

  void insert_without_grow(T *value) {
    size_t idx = index_for(value);
    while (true) {
      if (buckets[idx] == EMPTY) {
        buckets[idx] = value;
        ++size_;
        return;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  void rehash(size_t requested_capacity) {
    size_t capacity = next_pow2(requested_capacity < 8 ? 8 : requested_capacity);
    std::vector<T *> old = std::move(buckets);
    buckets.assign(capacity, EMPTY);
    size_ = 0;
    tombstones_ = 0;
    for (T *value : old) {
      if (value != EMPTY && value != TOMBSTONE) {
        insert_without_grow(value);
      }
    }
  }

  void maybe_rehash_for_insert() {
    if (buckets.empty()) {
      rehash(8);
      return;
    }
    if (size_ + tombstones_ + 1 > buckets.size() / 2) {
      size_t grown_capacity =
          buckets.size() > std::numeric_limits<size_t>::max() / 2
              ? std::numeric_limits<size_t>::max()
              : buckets.size() * 2;
      rehash(grown_capacity);
    } else if (tombstones_ > size_) {
      rehash(buckets.size());
    }
  }

public:
  class iterator {
    PointerHashSetImpl *set_{nullptr};
    size_t idx_{0};

    void skip_to_live() {
      while (idx_ < set_->buckets.size()) {
        T *value = set_->buckets[idx_];
        if (value != EMPTY && value != TOMBSTONE) {
          break;
        }
        ++idx_;
      }
    }

  public:
    using value_type = T *;
    using difference_type = std::ptrdiff_t;

    iterator(PointerHashSetImpl *set, size_t idx)
        : set_(set), idx_(idx) {
      if (set_) {
        skip_to_live();
      }
    }

    T *operator*() const { return set_->buckets[idx_]; }

    iterator &operator++() {
      ++idx_;
      skip_to_live();
      return *this;
    }

    bool operator==(const iterator &other) const {
      return set_ == other.set_ && idx_ == other.idx_;
    }
  };

  PointerHashSetImpl() { rehash(8); }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, buckets.size()); }

  size_t size() const { return size_; }

  void clear() {
    for (auto &bucket : buckets) {
      bucket = EMPTY;
    }
    size_ = 0;
    tombstones_ = 0;
  }

  void reserve(size_t n) {
    size_t needed = (n < 4 ? 8 : n * 2);
    if (needed > buckets.size()) {
      rehash(needed);
    }
  }

  bool insert(T *value) {
    maybe_rehash_for_insert();
    size_t idx = index_for(value);
    size_t first_tombstone = buckets.size();

    while (true) {
      T *entry = buckets[idx];
      if (entry == value) {
        return false;
      }
      if (entry == TOMBSTONE && first_tombstone == buckets.size()) {
        first_tombstone = idx;
      }
      if (entry == EMPTY) {
        size_t insert_idx =
            first_tombstone != buckets.size() ? first_tombstone : idx;
        if (buckets[insert_idx] == TOMBSTONE) {
          --tombstones_;
        }
        buckets[insert_idx] = value;
        ++size_;
        return true;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  bool erase(T *value) {
    size_t idx = index_for(value);
    while (true) {
      T *entry = buckets[idx];
      if (entry == EMPTY) {
        return false;
      }
      if (entry == value) {
        buckets[idx] = TOMBSTONE;
        --size_;
        ++tombstones_;
        return true;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }
};

template <typename T>
using PointerHashSet =
    std::conditional_t<Has64BitPointer<T>, PointerHashSetImpl<T>,
                       std::unordered_set<T *>>;
