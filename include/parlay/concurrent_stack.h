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

#include <cstdio>
#include <cstdint>
#include <iostream>
#include "utilities.h"

namespace parlay {

template<typename T>
class concurrent_stack {

  struct Node {
    T value;
    Node* next;
    size_t length;
  };

  class alignas(64) prim_concurrent_stack {
    struct nodeAndCounter {
      Node* node;
      uint64_t counter;
    };

    union CAS_t {
      __uint128_t x;
      nodeAndCounter NC;
    };
    CAS_t head;

    size_t length(Node* n) {
      if (n == NULL) return 0;
      else return n->length;
    }

  public:
    prim_concurrent_stack() {
      head.NC.node = NULL;
      head.NC.counter = 0;
      std::atomic_thread_fence(std::memory_order_seq_cst);
    }

    size_t size() {
      return length(head.NC.node);}

    void push(Node* newNode){
      CAS_t oldHead, newHead;
      do {
	oldHead = head;
	newNode->next = oldHead.NC.node;
	newNode->length = length(oldHead.NC.node) + 1;
	//std::atomic_thread_fence(std::memory_order_release);
	std::atomic_thread_fence(std::memory_order_seq_cst);
	newHead.NC.node = newNode;
	newHead.NC.counter = oldHead.NC.counter + 1;
      } while (!__sync_bool_compare_and_swap_16(&head.x,oldHead.x, newHead.x));
    }
    Node* pop() {
      Node* result;
      CAS_t oldHead, newHead;
      do {
	oldHead = head;
	result = oldHead.NC.node;
	if (result == NULL) return result;
	newHead.NC.node = result->next;
	newHead.NC.counter = oldHead.NC.counter + 1;
      } while (!__sync_bool_compare_and_swap_16(&head.x,oldHead.x, newHead.x));

      return result;
    }
  };// __attribute__((aligned(16)));

  prim_concurrent_stack a;
  prim_concurrent_stack b;

 public:

  size_t size() { return a.size();}

  void push(T v) {
    Node* x = b.pop();
    if (!x) x = (Node*) malloc(sizeof(Node));
    x->value = v;
    a.push(x);
  }

  maybe<T> pop() {
    Node* x = a.pop();
    if (!x) return maybe<T>();
    T r = x->value;
    b.push(x);
    return maybe<T>(r);
  }

  // assumes no push or pop in progress
  void clear() {
    Node* x;
    while ((x = a.pop())) free(x);
    while ((x = b.pop())) free(x);
  }

  concurrent_stack() {}
  ~concurrent_stack() { clear();}
};

}  // namespace parlay

#endif  // PARLAY_CONCURRENT_STACK_H_
