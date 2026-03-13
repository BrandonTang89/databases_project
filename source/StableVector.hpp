#pragma once

#include <cstddef>
#include <memory>
#include <utility>

// StableVector<T, numPerNode>
//
// A container backed by a linked list of fixed-size arrays (nodes).
// Pointers/references to elements are never invalidated by push_back/emplace.
//
// push_back(T&&)  -> T*
// emplace(Args&&...) -> T*

template <typename T, size_t NumPerNode> class StableVector {
  static_assert(NumPerNode > 0, "NumPerNode must be greater than 0");

  struct Node {
    // Raw storage — avoids default-constructing T for every slot.
    alignas(T) std::byte storage[sizeof(T) * NumPerNode];
    size_t count = 0;
    std::unique_ptr<Node> next;

    T *slot(size_t i) { return reinterpret_cast<T *>(storage) + i; }

    ~Node() {
      for (size_t i = 0; i < count; ++i) {
        slot(i)->~T();
      }
    }
  };

public:
  StableVector() = default;

  // Non-copyable; moving is allowed.
  StableVector(const StableVector &) = delete;
  StableVector &operator=(const StableVector &) = delete;

  StableVector(StableVector &&) = default;
  StableVector &operator=(StableVector &&) = default;

  ~StableVector() = default;

  // push_back: move-construct from t
  T *push_back(T &&t) { return emplace(std::move(t)); }

  // emplace: construct T in-place from args
  template <typename... Args> T *emplace(Args &&...args) {
    ensure_space();
    T *ptr = tail_->slot(tail_->count);
    new (ptr) T(std::forward<Args>(args)...);
    ++tail_->count;
    ++size_;
    return ptr;
  }

  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

private:
  void ensure_space() {
    if (!head_) {
      head_ = std::make_unique<Node>();
      tail_ = head_.get();
      return;
    }
    if (tail_->count == NumPerNode) {
      tail_->next = std::make_unique<Node>();
      tail_ = tail_->next.get();
    }
  }

  std::unique_ptr<Node> head_;
  Node *tail_ = nullptr;
  size_t size_ = 0;
};
