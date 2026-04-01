#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>

inline uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

inline size_t next_pow2(size_t n) {
  constexpr size_t max_pow2 =
      size_t{1} << (std::numeric_limits<size_t>::digits - 1);
  if (n >= max_pow2) {
    return max_pow2;
  }
  return std::bit_ceil(n);
}
