#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

constexpr uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

constexpr size_t next_pow2(size_t n) {
  constexpr size_t max_pow2 = size_t{1}
                              << (std::numeric_limits<size_t>::digits - 1);
  if (n >= max_pow2) {
    return max_pow2;
  }
  return std::bit_ceil(n);
}

namespace database {

template <typename K> struct hash {
  static size_t operator()(const K &key) noexcept {
    return std::hash<K>{}(key);
  }

  static K empty() noexcept {
    static_assert(std::is_integral_v<K>,
                  "OpenAddrHashMap requires integral or pointer keys when "
                  "using sentinel buckets");
    return std::numeric_limits<K>::max();
  }

  static K tombstone() noexcept {
    static_assert(std::is_integral_v<K>,
                  "OpenAddrHashMap requires integral or pointer keys when "
                  "using sentinel buckets");
    return std::numeric_limits<K>::max() - 1;
  }
};

template <> struct hash<uint64_t> {
  static size_t operator()(const uint64_t &key) noexcept {
    return static_cast<size_t>(splitmix64(key));
  }

  static uint64_t empty() noexcept {
    return std::numeric_limits<uint64_t>::max();
  }

  static uint64_t tombstone() noexcept {
    return std::numeric_limits<uint64_t>::max() - 1;
  }
};

template <typename T> struct hash<T *> {
  static size_t operator()(const T *const &key) noexcept {
    const auto raw = reinterpret_cast<std::uintptr_t>(key);
    if constexpr (sizeof(std::uintptr_t) == sizeof(uint64_t)) {
      return static_cast<size_t>(splitmix64(static_cast<uint64_t>(raw)));
    }
    return std::hash<std::uintptr_t>{}(raw);
  }

  static T *empty() { return nullptr; }

  static T *tombstone() { return reinterpret_cast<T *>(~std::uintptr_t{0}); }
};

} // namespace database
