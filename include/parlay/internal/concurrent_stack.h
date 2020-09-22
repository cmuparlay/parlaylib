// Lock free, linearizable implementation of a concurrent stack
// supporting:
//    push
//    pop
//    size
// Works for elements of any type T
// It requires memory proportional to the largest it has been
// This can be cleared, but only when noone else is using it.
// Requires 128-bit-compare-and-swap
// Counter could overflow "in theory", but would require over 500 years even
// if updated every nanosecond (and must be updated sequentially)

#ifndef PARLAY_CONCURRENT_STACK_H_
#define PARLAY_CONCURRENT_STACK_H_

#include <cstdint>
#include <cstdio>

#include <atomic>
#include <iostream>
#include <mutex>
#include <optional>

#include "../utilities.h"

namespace parlay {

template <typename T>
class concurrent_stack {
  struct Node {
    T value;
    Node* next;
    size_t length;
  };

  class alignas(64) locking_concurrent_stack {

    Node* head;
    std::mutex stack_mutex;

    size_t length(Node* n) {
      if (n == nullptr) {
        return 0;
      }
      else {
        return n->length;
      }
    }

   public:
    locking_concurrent_stack() : head(nullptr) {

    }

    size_t size() { return length(head); }

    void push(Node* newNode) {
      std::lock_guard<std::mutex> lock(stack_mutex);
      newNode->next = head;
      newNode->length  = length(head) + 1;
      head = newNode;
    }
    
    Node* pop() {
      std::lock_guard<std::mutex> lock(stack_mutex);
      Node* result = head;
      if (head != nullptr) head = head->next;
      return result;
    }
  };

  locking_concurrent_stack a;
  locking_concurrent_stack b;

 public:
  size_t size() { return a.size(); }

  void push(T v) {
    Node* x = b.pop();
    if (!x) x = (Node*) ::operator new(sizeof(Node));
    x->value = v;
    a.push(x);
  }

  std::optional<T> pop() {
    Node* x = a.pop();
    if (!x) return {};
    T r = x->value;
    b.push(x);
    return {r};
  }

  // assumes no push or pop in progress
  void clear() {
    Node* x;
    while ((x = a.pop())) ::operator delete(x);
    while ((x = b.pop())) ::operator delete(x);
  }

  concurrent_stack() {}
  ~concurrent_stack() { clear(); }
};

}  // namespace parlay

#endif  // PARLAY_CONCURRENT_STACK_H_
