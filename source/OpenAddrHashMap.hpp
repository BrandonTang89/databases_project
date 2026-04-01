#pragma once

#include "hashtable_utils.hpp"
#include <cassert>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

template <typename K, typename V, typename Hasher = database::hash<K>>
class OpenAddrHashMap {
  struct Bucket {
    K first{};
    V second{};
  };

  std::vector<Bucket> buckets;
  size_t size_{0};
  [[no_unique_address]] Hasher hasher_{};
  K empty_key_;

  bool is_empty(const Bucket &bucket) const {
    return bucket.first == empty_key_;
  }

  void mark_empty(Bucket &bucket) {
    bucket.first = empty_key_;
    bucket.second = V{};
  }

  size_t index_for(const K &key) const {
    return hasher_(key) & (buckets.size() - 1);
  }

  void maybe_rehash_for_insert() {
    if (buckets.empty()) {
      rehash(8);
      return;
    }
    if (size_ + 1 > buckets.size() / 2) {
      size_t grown_capacity =
          buckets.size() > std::numeric_limits<size_t>::max() / 2
              ? std::numeric_limits<size_t>::max()
              : buckets.size() * 2;
      rehash(grown_capacity);
    }
  }

  template <typename Key, typename Mapped>
  size_t insert_without_grow(Key &&key, Mapped &&value) {
    size_t idx = index_for(key);
    while (true) {
      Bucket &bucket = buckets[idx];
      if (is_empty(bucket)) {
        bucket.first = std::forward<Key>(key);
        bucket.second = std::forward<Mapped>(value);
        ++size_;
        return idx;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  void rehash(size_t requested_capacity) {
    size_t capacity = next_pow2(requested_capacity < 8 ? 8 : requested_capacity);
    std::vector<Bucket> old = std::move(buckets);
    buckets.clear();
    buckets.resize(capacity);
    size_ = 0;
    for (Bucket &bucket : buckets) {
      mark_empty(bucket);
    }
    for (Bucket &bucket : old) {
      if (!is_empty(bucket)) {
        insert_without_grow(std::move(bucket.first), std::move(bucket.second));
      }
    }
  }

public:
  class iterator {
    OpenAddrHashMap *map_{nullptr};
    size_t idx_{0};

    void skip_to_live() {
      while (idx_ < map_->buckets.size()) {
        if (!map_->is_empty(map_->buckets[idx_])) {
          break;
        }
        ++idx_;
      }
    }

  public:
    using value_type = Bucket;
    using difference_type = std::ptrdiff_t;

    iterator() = default;

    iterator(OpenAddrHashMap *map, size_t idx) : map_(map), idx_(idx) {
      if (map_ != nullptr) {
        skip_to_live();
      }
    }

    value_type &operator*() const {
      assert(map_ && !map_->is_empty(map_->buckets[idx_]));
      return map_->buckets[idx_];
    }

    value_type *operator->() const {
      assert(map_ && !map_->is_empty(map_->buckets[idx_]));
      return &map_->buckets[idx_];
    }

    iterator &operator++() {
      ++idx_;
      skip_to_live();
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const iterator &other) const {
      return map_ == other.map_ && idx_ == other.idx_;
    }
  };

  class const_iterator {
    const OpenAddrHashMap *map_{nullptr};
    size_t idx_{0};

    void skip_to_live() {
      while (idx_ < map_->buckets.size()) {
        if (!map_->is_empty(map_->buckets[idx_])) {
          break;
        }
        ++idx_;
      }
    }

  public:
    using value_type = const Bucket;
    using difference_type = std::ptrdiff_t;

    const_iterator() = default;

    const_iterator(const OpenAddrHashMap *map, size_t idx)
        : map_(map), idx_(idx) {
      if (map_ != nullptr) {
        skip_to_live();
      }
    }

    value_type &operator*() const {
      assert(map_ && !map_->is_empty(map_->buckets[idx_]));
      return map_->buckets[idx_];
    }

    value_type *operator->() const {
      assert(map_ && !map_->is_empty(map_->buckets[idx_]));
      return &map_->buckets[idx_];
    }

    const_iterator &operator++() {
      ++idx_;
      skip_to_live();
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const const_iterator &other) const {
      return map_ == other.map_ && idx_ == other.idx_;
    }
  };

  OpenAddrHashMap() : empty_key_(hasher_.empty()) { rehash(8); }

  size_t size() const { return size_; }

  void clear() {
    for (Bucket &bucket : buckets) {
      mark_empty(bucket);
    }
    size_ = 0;
  }

  void reserve(size_t n) {
    size_t needed = (n < 4 ? 8 : n * 2);
    if (needed > buckets.size()) {
      rehash(needed);
    }
  }

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, buckets.size()); }

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, buckets.size()); }

  iterator find(const K &key) {
    assert(key != empty_key_ && "key collides with empty sentinel");
    size_t idx = index_for(key);
    while (true) {
      Bucket &bucket = buckets[idx];
      if (is_empty(bucket)) {
        return end();
      }
      if (bucket.first == key) {
        return iterator(this, idx);
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  const_iterator find(const K &key) const {
    assert(key != empty_key_ && "key collides with empty sentinel");
    size_t idx = index_for(key);
    while (true) {
      const Bucket &bucket = buckets[idx];
      if (is_empty(bucket)) {
        return end();
      }
      if (bucket.first == key) {
        return const_iterator(this, idx);
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  V &at(const K &key) {
    auto it = find(key);
    assert(it != end() && "key not found");
    return it->second;
  }

  const V &at(const K &key) const {
    auto it = find(key);
    assert(it != end() && "key not found");
    return it->second;
  }

  template <typename Key, typename... Args>
    requires std::constructible_from<K, Key &&> &&
             std::constructible_from<V, Args &&...>
  std::pair<iterator, bool> emplace(Key &&key, Args &&...args) {
    assert(key != empty_key_ && "key collides with empty sentinel");
    auto it = find(key);
    if (it != end()) {
      return {it, false};
    }

    maybe_rehash_for_insert();

    size_t idx = index_for(key);
    while (true) {
      Bucket &bucket = buckets[idx];
      if (is_empty(bucket)) {
        bucket.first = std::forward<Key>(key);
        bucket.second = V(std::forward<Args>(args)...);
        ++size_;
        return {iterator(this, idx), true};
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  V &operator[](const K &key)
    requires std::default_initializable<V>
  {
    assert(key != empty_key_ && "key collides with empty sentinel");
    maybe_rehash_for_insert();

    size_t idx = index_for(key);
    while (true) {
      Bucket &bucket = buckets[idx];
      if (!is_empty(bucket) && bucket.first == key) {
        return bucket.second;
      }
      if (is_empty(bucket)) {
        bucket.first = key;
        bucket.second = V{};
        ++size_;
        return bucket.second;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  V &operator[](K &&key)
    requires std::default_initializable<V>
  {
    assert(key != empty_key_ && "key collides with empty sentinel");
    maybe_rehash_for_insert();

    size_t idx = index_for(key);
    while (true) {
      Bucket &bucket = buckets[idx];
      if (!is_empty(bucket) && bucket.first == key) {
        return bucket.second;
      }
      if (is_empty(bucket)) {
        bucket.first = std::move(key);
        bucket.second = V{};
        ++size_;
        return bucket.second;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }
};
