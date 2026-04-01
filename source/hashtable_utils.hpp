#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <type_traits>

inline uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

inline size_t next_pow2(size_t n) {
  constexpr size_t max_pow2 = size_t{1}
                              << (std::numeric_limits<size_t>::digits - 1);
  if (n >= max_pow2) {
    return max_pow2;
  }
  return std::bit_ceil(n);
}

namespace database {

template <typename K> struct hash {
  size_t operator()(const K &key) const noexcept { return std::hash<K>{}(key); }

  K empty() const noexcept {
    static_assert(std::is_integral_v<K>,
                  "OpenAddrHashMap requires integral or pointer keys when "
                  "using sentinel buckets");
    return std::numeric_limits<K>::max();
  }

  K tombstone() const noexcept {
    static_assert(std::is_integral_v<K>,
                  "OpenAddrHashMap requires integral or pointer keys when "
                  "using sentinel buckets");
    return std::numeric_limits<K>::max() - 1;
  }
};

template <> struct hash<uint64_t> {
  size_t operator()(const uint64_t &key) const noexcept {
    return static_cast<size_t>(splitmix64(key));
  }

  uint64_t empty() const noexcept {
    return std::numeric_limits<uint64_t>::max();
  }

  uint64_t tombstone() const noexcept {
    return std::numeric_limits<uint64_t>::max() - 1;
  }
};

template <typename T> struct hash<T *> {
  size_t operator()(T *const &key) const noexcept {
    const std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(key);
    if constexpr (sizeof(std::uintptr_t) == sizeof(uint64_t)) {
      return static_cast<size_t>(splitmix64(static_cast<uint64_t>(raw)));
    }
    return std::hash<std::uintptr_t>{}(raw);
  }

  T *empty() const noexcept { return nullptr; }

  T *tombstone() { return reinterpret_cast<T *>(~std::uintptr_t{0}); }
};

} // namespace database
