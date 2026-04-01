#pragma once

#include "hashtable_utils.hpp"
#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

template <typename K, typename Hasher = database::hash<K>>
class OpenAddrHashSet {
  std::vector<K> buckets;
  size_t size_{0};
  size_t tombstones_{0};
  [[no_unique_address]] Hasher hasher_{};
  K empty_key_;
  K tombstone_key_;

  bool is_empty(const K &value) const { return value == empty_key_; }
  bool is_tombstone(const K &value) const { return value == tombstone_key_; }
  bool is_live(const K &value) const {
    return !is_empty(value) && !is_tombstone(value);
  }

  size_t index_for(const K &value) const {
    return hasher_(value) & (buckets.size() - 1);
  }

  void set_empty(K &value) { value = empty_key_; }

  void insert_without_grow(const K &value) {
    size_t idx = index_for(value);
    while (true) {
      if (is_empty(buckets[idx])) {
        buckets[idx] = value;
        ++size_;
        return;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  void rehash(size_t requested_capacity) {
    size_t capacity = next_pow2(requested_capacity < 8 ? 8 : requested_capacity);
    std::vector<K> old = std::move(buckets);
    buckets.assign(capacity, empty_key_);
    size_ = 0;
    tombstones_ = 0;
    for (const K &value : old) {
      if (is_live(value)) {
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
    OpenAddrHashSet *set_{nullptr};
    size_t idx_{0};

    void skip_to_live() {
      while (idx_ < set_->buckets.size()) {
        if (set_->is_live(set_->buckets[idx_])) {
          break;
        }
        ++idx_;
      }
    }

  public:
    using value_type = K;
    using difference_type = std::ptrdiff_t;

    iterator() = default;

    iterator(OpenAddrHashSet *set, size_t idx) : set_(set), idx_(idx) {
      if (set_ != nullptr) {
        skip_to_live();
      }
    }

    const K &operator*() const { return set_->buckets[idx_]; }

    iterator &operator++() {
      ++idx_;
      skip_to_live();
      return *this;
    }

    bool operator==(const iterator &other) const {
      return set_ == other.set_ && idx_ == other.idx_;
    }
  };

  OpenAddrHashSet()
      : empty_key_(database::hash<K>{}.empty()),
        tombstone_key_(database::hash<K>{}.tombstone()) {
    assert(empty_key_ != tombstone_key_ && "sentinel keys must be distinct");
    rehash(8);
  }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, buckets.size()); }

  size_t size() const { return size_; }

  void clear() {
    for (K &bucket : buckets) {
      set_empty(bucket);
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

  bool contains(const K &value) const {
    assert(value != empty_key_ && value != tombstone_key_ &&
           "key collides with sentinel");
    size_t idx = index_for(value);
    while (true) {
      const K &entry = buckets[idx];
      if (is_empty(entry)) {
        return false;
      }
      if (entry == value) {
        return true;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  bool insert(const K &value) {
    assert(value != empty_key_ && value != tombstone_key_ &&
           "key collides with sentinel");
    maybe_rehash_for_insert();
    size_t idx = index_for(value);
    size_t first_tombstone = buckets.size();

    while (true) {
      K &entry = buckets[idx];
      if (entry == value) {
        return false;
      }
      if (is_tombstone(entry) && first_tombstone == buckets.size()) {
        first_tombstone = idx;
      }
      if (is_empty(entry)) {
        size_t insert_idx =
            first_tombstone != buckets.size() ? first_tombstone : idx;
        if (is_tombstone(buckets[insert_idx])) {
          --tombstones_;
        }
        buckets[insert_idx] = value;
        ++size_;
        return true;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  bool erase(const K &value) {
    assert(value != empty_key_ && value != tombstone_key_ &&
           "key collides with sentinel");
    size_t idx = index_for(value);
    while (true) {
      K &entry = buckets[idx];
      if (is_empty(entry)) {
        return false;
      }
      if (entry == value) {
        entry = tombstone_key_;
        --size_;
        ++tombstones_;
        return true;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }
};
