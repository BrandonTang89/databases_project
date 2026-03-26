#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

template <typename V> class OpenAddressingHashMap {
  static constexpr uint64_t EMPTY_KEY = 0xFFFFFFFF00000000ULL;

  struct Bucket {
    uint64_t first{EMPTY_KEY};
    V second{};
  };

  std::vector<Bucket> buckets;
  size_t size_{0};

  static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
  }

  static size_t next_pow2(size_t n) {
    constexpr size_t max_pow2 =
        size_t{1} << (std::numeric_limits<size_t>::digits - 1);
    if (n >= max_pow2) {
      return max_pow2;
    }
    return std::bit_ceil(n);
  }

  size_t index_for(uint64_t key) const {
    return static_cast<size_t>(
        splitmix64(key) & static_cast<uint64_t>(buckets.size() - 1));
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

  void insert_without_grow(uint64_t key, V value) {
    size_t idx = index_for(key);
    while (true) {
      auto &bucket = buckets[idx];
      if (bucket.first == EMPTY_KEY) {
        bucket.first = key;
        bucket.second = std::move(value);
        ++size_;
        return;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  void rehash(size_t requested_capacity) {
    size_t capacity = next_pow2(requested_capacity < 8 ? 8 : requested_capacity);
    std::vector<Bucket> old = std::move(buckets);
    buckets.assign(capacity, Bucket{});
    size_ = 0;
    for (auto &bucket : old) {
      if (bucket.first != EMPTY_KEY) {
        insert_without_grow(bucket.first, std::move(bucket.second));
      }
    }
  }

public:
  class iterator {
    OpenAddressingHashMap *map_{nullptr};
    size_t idx_{0};

    void skip_to_live() {
      while (idx_ < map_->buckets.size()) {
        uint64_t key = map_->buckets[idx_].first;
        if (key != EMPTY_KEY) {
          break;
        }
        ++idx_;
      }
    }

  public:
    using value_type = Bucket;
    using difference_type = std::ptrdiff_t;

    iterator() = default;

    iterator(OpenAddressingHashMap *map, size_t idx) : map_(map), idx_(idx) {
      if (map_) {
        skip_to_live();
      }
    }

    value_type &operator*() const { return map_->buckets[idx_]; }
    value_type *operator->() const { return &map_->buckets[idx_]; }

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

  OpenAddressingHashMap() { rehash(8); }

  size_t size() const { return size_; }

  void clear() {
    for (auto &bucket : buckets) {
      bucket.first = EMPTY_KEY;
      bucket.second = V{};
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

  iterator find(uint64_t key) {
    size_t idx = index_for(key);
    while (true) {
      auto &bucket = buckets[idx];
      if (bucket.first == EMPTY_KEY) {
        return end();
      }
      if (bucket.first == key) {
        return iterator(this, idx);
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }

  V &operator[](uint64_t key) {
    maybe_rehash_for_insert();

    size_t idx = index_for(key);

    while (true) {
      auto &bucket = buckets[idx];
      if (bucket.first == key) {
        return bucket.second;
      }
      if (bucket.first == EMPTY_KEY) {
        bucket.first = key;
        bucket.second = V{};
        ++size_;
        return bucket.second;
      }
      idx = (idx + 1) & (buckets.size() - 1);
    }
  }
};
