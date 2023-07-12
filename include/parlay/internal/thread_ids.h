// MIT license (https://opensource.org/license/mit/)
// Initial Author: Guy Blelloch
// Developed as part of cmuparlay/parlaylib

// Supports unique thread ids for std:threads.  Supports
//   thread_id.get() -- returns a unique, and fixed, id for the thread
//   thread_id.max_current_id() -- returns an upper bound on any thread id up to the current time
//   const thread_id.max_id -- returns an upper bound on any id ever used
// The id will be allocated on first use within an std::thread and
// retired when thread finishes (if one was allocated).
// Ids are allocated starting at 0 and going up.
// When a thread finishes its id is then available for reuse.
// The value returned by max_id() never decreases.
// Currently limit is max_id=1024 concurrent thread ids.

#ifndef PARLAY_THREAD_IDS_
#define PARLAY_THREAD_IDS_

#include <atomic>
#include <vector>

namespace parlay {
  namespace internal {

static constexpr int max_thread_ids = 1024;

// Maintains a pool of ids which threads can grab a unique id from. In
// particular it atomically maintains a boolean vector marked as full
// (true) in index i if id i is being used.  When asking for a new id
// using add_id(), it looks for the first empty (false) slot, marks it
// as full (true) and returns its index.  When giving up an id with
// remove_id(int i) it sets slot i to empty (false).
struct thread_id_pool {
private:
  std::vector<std::atomic<bool>> id_slots;
  std::atomic<long> max_used ;

public:
  long max_id() {return max_used.load();}
  
  long add_id() {
    long i;
    bool old;
    do {
      old = false;
      for (i=0; (i < max_thread_ids) && id_slots[i]; i++);
      assert(i < max_thread_ids);
    } while (!id_slots[i].compare_exchange_strong(old, true));
    long max_id_used = max_used.load();
    while (i > max_id_used &&
	   max_used.compare_exchange_strong(max_id_used, i));
    //std::cout << "adding: " << i << std::endl;
    return i;
  }

  void remove_id(long i) {
    //std::cout << "removing: " << i << std::endl;
    id_slots[i] = false; }

  thread_id_pool() :
    id_slots(std::vector<std::atomic<bool>>(max_thread_ids)),
    max_used(0) {
    std::fill(id_slots.begin(), id_slots.end(), false); 
  }
};

extern inline thread_id_pool& thread_ids() {
  static thread_id_pool global_thread_id_pool;
  return global_thread_id_pool;
}

struct thread_id_t {
  long id;
  long get() {
    if (id == -1) id = thread_ids().add_id();
    return id;
  }
  static long max_thread_id() { return max_thread_ids;}
  static long max_current_id() {return thread_ids().max_id(); }
  thread_id_t() : id(-1) {}
  ~thread_id_t() { if (id != -1) thread_ids().remove_id(id);}
};

extern inline thread_id_t& thread_id() {
  static thread_local thread_id_t my_thread_id;
  return my_thread_id;
}

}  // namespace internal
} // namespace parlay

#endif // PARLAY_THREAD_IDS_
