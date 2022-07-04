
#ifndef PARLAY_INTERNAL_CONCURRENCY_HP_STACK_H
#define PARLAY_INTERNAL_CONCURRENCY_HP_STACK_H

#include <atomic>
#include <optional>
#include <utility>

#include "acquire_retire.h"

namespace parlay {
namespace internal {
namespace concurrency {

// A low-level lock-free concurrent stack that re-uses its allocated nodes rather
// than freeing them. That is, the memory used by the stack is proportional
// to the largest that it has ever been.
//
// More specifically, multiple instances of the same type of stack actually
// share the same pool of nodes, so the total memory used is proportional
// to the maximum amount of memory ever used by all the stacks at any given
// point.
//
// This is used to implement the Parlay allocator, so the idea is to not
// have the stack frequently allocating, or it would defeat the purpose
// of the allocator!
//
// To avoid the ABA problem, the stack uses acquire_retire (i.e. hazard-pointers)
// to delay when a popped node is allowed to be subsequently re-used.
//
// Template parameters:
//  T:  The type of the element stored in the stack
template<typename T>
class hp_stack {

  struct Node {
    T t;
    Node* next;
    size_t length;

    explicit Node(T t_) noexcept(std::is_nothrow_move_constructible_v<T>)
      : t(std::move(t_)), next(nullptr), length(1) {}
    Node(T t_, Node* next_) noexcept(std::is_nothrow_move_constructible_v<T>)
      : t(std::move(t_)), next(next_), length(next ? next->length + 1 : 1) {}
  };

  // An internal stack used by the enclosing stack class to hold Nodes for
  // reuse rather than freeing them. This is useful since this low-level stack
  // is used to implement Parlay's allocator, so we don't want to be allocating
  // new nodes too often, or we will hurt the allocator performance
  //
  // in other words, yo dawg, I heard you like stacks so we put a stack inside your stack
  struct alignas(64) node_stack {
    node_stack() noexcept : head(nullptr) {}

    void push(Node* p) noexcept {
      while (!head.compare_exchange_weak(p->next, p)) { }
    }

    Node* pop() noexcept {
      Node* p;
      do {
        p = head.load();
        if (p == nullptr) return p;
      } while (!head.compare_exchange_weak(p, p->next));
      return p;
    }

    ~node_stack() {
      Node* p = head.load();
      Node* prev = nullptr;
      while (p) {
        prev = p;
        p = p->next;
        delete prev;
      }
    }

    std::atomic<Node*> head;
  };

 public:
  hp_stack() : head(nullptr) {}

  void push(T t) {
    auto p = node_stack_instance().pop();
    if (!p) p = static_cast<Node*>(::operator new(sizeof(Node)));
    new (p) Node(t, head.load());
    while (!head.compare_exchange_weak(p->next, p)) { }
  }

  std::optional<T> pop() {
    Node* p;
    do {
      p = hp_instance().acquire(&head);
      if (p == nullptr) return {};
    } while (!head.compare_exchange_weak(p, p->next));
    auto val = std::make_optional(std::move(p->t));
    p->~Node();
    hp_instance().retire(p);
    hp_instance().release();
    return val;
  }

  [[nodiscard]] size_t size() const noexcept {
    auto p = head.load();
    return p ? p->length : 0;
  }

  [[nodiscard]] bool empty() const noexcept {
    return head.load() == nullptr;
  }

  ~hp_stack() {
    Node* p = head.load();
    Node* prev = nullptr;
    while (p) {
      prev = p;
      p = p->next;
      delete prev;
    }
  }

  void clear() {
    while (pop()) { }
  }

 private:

  std::atomic<Node*> head;

  // We use the custom deleter feature of acquire-retire to delay the re-use
  // of nodes once they have been popped. This helps us to avoid the ABA problem.
  // The idea is that once a node is popped, it doesn't go back in the re-use
  // stack until after any concurrent pops are completed. The role of the custom
  // deleter is therefore to push the node onto the re-use stack once this is true.
  struct Deleter {
    void operator()(Node* p) {
      node_stack_instance().push(p);
    }
  };

  static acquire_retire<Node, Deleter>& hp_instance() {
    static acquire_retire<Node, Deleter> instance;
    return instance;
  }

  static node_stack& node_stack_instance() {
    static node_stack instance;
    return instance;
  }
};

}  // namespace concurrency
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_CONCURRENCY_HP_STACK_H
