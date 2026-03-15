#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>

// StableVector<T, numPerNode>
//
// A container backed by a linked list of fixed-size arrays (nodes).
// Pointers/references to elements are never invalidated by push_back/emplace.
//
// push_back(T&&)  -> T*
// emplace(Args&&...) -> T*

template <typename T, size_t NumPerNode = 1> class StableVector {
  static_assert(NumPerNode > 0, "NumPerNode must be greater than 0");

  struct Node {
    // Raw storage — avoids default-constructing T for every slot.
    alignas(T) std::byte storage[sizeof(T) * NumPerNode];
    size_t count = 0;
    Node *prev = nullptr;
    std::unique_ptr<Node> next;

    T *slot(size_t i) { return reinterpret_cast<T *>(storage) + i; }
    const T *slot(size_t i) const {
      return reinterpret_cast<const T *>(storage) + i;
    }

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

  // -----------------------------------------------------------------------
  // Iterator
  // -----------------------------------------------------------------------
  template <bool IsConst> class Iterator {
    using NodePtr = std::conditional_t<IsConst, const Node *, Node *>;
    using ValueRef = std::conditional_t<IsConst, const T &, T &>;
    using ValuePtr = std::conditional_t<IsConst, const T *, T *>;

    NodePtr node_ = nullptr; // nullptr == end()
    size_t  idx_  = 0;
    NodePtr tail_ = nullptr; // needed for --end()

  public:
    // Standard iterator traits
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = ValuePtr;
    using reference         = ValueRef;

    Iterator() = default;
    Iterator(NodePtr node, size_t idx, NodePtr tail)
        : node_(node), idx_(idx), tail_(tail) {}

    // Allow implicit conversion from iterator to const_iterator
    operator Iterator<true>() const requires (!IsConst) {
      return Iterator<true>(node_, idx_, tail_);
    }

    ValueRef operator*()  const { return *node_->slot(idx_); }
    ValuePtr operator->() const { return  node_->slot(idx_); }

    // prefix ++
    Iterator &operator++() {
      ++idx_;
      if (idx_ == node_->count) {
        node_ = node_->next.get(); // nullptr when we were on the last node
        idx_  = 0;
      }
      return *this;
    }

    // postfix ++
    Iterator operator++(int) {
      Iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    // prefix --
    Iterator &operator--() {
      if (idx_ > 0) {
        --idx_;
      } else {
        // node_ == nullptr means we are at end(); step back to tail_.
        node_ = (node_ == nullptr) ? tail_ : node_->prev;
        idx_  = node_->count - 1;
      }
      return *this;
    }

    // postfix --
    Iterator operator--(int) {
      Iterator tmp = *this;
      --(*this);
      return tmp;
    }

    bool operator==(const Iterator &o) const {
      return node_ == o.node_ && idx_ == o.idx_;
    }
    bool operator!=(const Iterator &o) const { return !(*this == o); }
  };

  using iterator = Iterator<false>;
  using const_iterator = Iterator<true>;

  iterator begin() {
    if (!head_)
      return end();
    return iterator(head_.get(), 0, tail_);
  }
  iterator end() { return iterator(nullptr, 0, tail_); }

  const_iterator begin() const {
    if (!head_)
      return end();
    return const_iterator(head_.get(), 0, tail_);
  }
  const_iterator end() const { return const_iterator(nullptr, 0, tail_); }

  const_iterator cbegin() const { return begin(); }
  const_iterator cend() const { return end(); }

  // -----------------------------------------------------------------------

  // push_back: move-construct from t
  T *push_back(T &&t) { return emplace(std::move(t)); }
  T *push_back(const T &t) { return emplace(t); }

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
      auto new_node = std::make_unique<Node>();
      new_node->prev = tail_;
      tail_->next = std::move(new_node);
      tail_ = tail_->next.get();
    }
  }

  std::unique_ptr<Node> head_;
  Node *tail_ = nullptr;
  size_t size_ = 0;
};
