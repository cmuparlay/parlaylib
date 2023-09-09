
#ifndef PARLAY_INTERNAL_CONCURRENCY_HAZPTR_STACK_H
#define PARLAY_INTERNAL_CONCURRENCY_HAZPTR_STACK_H

#include <cstddef>

#include <atomic>
#include <optional>
#include <utility>  // IWYU pragma: keep

#include "acquire_retire.h"

namespace parlay {
namespace internal {

// A minimal, low-level, linearizable, lock-free concurrent
// stack that uses hazard-pointers for garbage collection.
//
// The stack keeps track of its current length
//
// Template parameters:
//  T:  The type of the element stored in the stack
template<typename T>
class hazptr_stack {

  struct Node {
    T t;
    Node* next;
    size_t length;

    // Allow the intrusive garbage collector to use the node's own next pointer
    // instead of having to allocate its own memory to store the retired nodes
    friend Node* intrusive_get_next(Node* node) {
      return node->next;
    }

    friend void intrusive_set_next(Node* node, Node* next_) {
      node->next = next_;
    }

    explicit Node(T t_) noexcept(std::is_nothrow_move_constructible_v<T>)
      : t(std::move(t_)), next(nullptr), length(1) { }
  };

 public:
  hazptr_stack() : head(nullptr) {
    // Touch the hazard pointer instance to force its initialization
    // before this object, since it must outlive this object. Without
    // this line, the hazard pointer instance might be destroyed before
    // the stack, in which case subsequent uses of the stack may try
    // to use the hazard pointer instance after it has been destroyed!
    hazptr_instance();
  }

  // Insert the object t at the top of the stack
  //
  // This operation is safe to perform concurrently with other operations
  void push(T t) {
    auto p = new Node(std::move(t));
    do {
      auto h = hazptr_instance().acquire(head);
      p->next = h;
      p->length = h ? h->length + 1 : 1;
    } while (!head.compare_exchange_weak(p->next, p));
    hazptr_instance().release();
  }

  // Remove and return the object at the top of the stack. If the stack
  // is empty, returns an empty optional<T>.
  //
  // This operation is safe to perform concurrently with other operations
  std::optional<T> pop() {
    Node* p;
    do {
      p = hazptr_instance().acquire(head);
      if (p == nullptr) return {};
    } while (!head.compare_exchange_weak(p, p->next));
    auto val = std::make_optional(std::move(p->t));
    hazptr_instance().retire(p);
    hazptr_instance().release();
    return val;
  }

  // Return the current size of the stack
  [[nodiscard]] size_t size() const noexcept {
    auto p = hazptr_instance().acquire(head);
    auto result = p ? p->length : 0;
    hazptr_instance().release();
    return result;
  }

  // Returns true if the stack is currently empty
  [[nodiscard]] bool empty() const noexcept {
    return head.load() == nullptr;
  }

  ~hazptr_stack() {
    // More efficient than clear since we can assume there are
    // no concurrent accesses and hence we don't need to use
    // pop, which acquires hazard pointers and uses a CAS loop
    Node* p = head.load();
    Node* prev = nullptr;
    while (p) {
      prev = p;
      p = p->next;
      delete prev;
    }
  }

  // Empties the stack
  //
  // Can be safely invoked concurrently with other operations, though it
  // is possible that this operation can be stalled indefinitely if other
  // threads continue to push onto the stack forever, then this thread
  // will just keep popping and never manage to empty it.
  void clear() {
    while (pop()) { }
  }

 private:
  std::atomic<Node*> head;

  static intrusive_acquire_retire<Node>& hazptr_instance() {
    static intrusive_acquire_retire<Node> instance;
    return instance;
  }
};

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_CONCURRENCY_HAZPTR_STACK_H
