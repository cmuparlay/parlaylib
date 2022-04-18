#pragma once

#include <limits>

#include <parlay/primitives.h>
#include <parlay/utilities.h>

// Uses the approach of deterministic reservations.  Basically it
// preserves the sequentiaal order of a loop, by running blocks
// (rounds) of iterations in parallel and checking for conflicts.  If
// there is potentially shared locations involving mutation among
// iterations, an iteration needs to "reserve" the locations.  Lower
// iterations have priority on the reservation.  After all iterations
// in the block try to reserve, a second pass can "check" if the
// iteration won on all its reservations, and apply itself if so.  Any
// iterations that fail are carried over to the next block.  The first
// iteration in a block always succeeds.
// See:
// "Internally deterministic parallel algorithms can be fast"
// Blelloch, Fineman, Gibbons, and Shun, 

// used to reserve a slot using write_min.
template <class idx>
struct reservation {
  std::atomic<idx> r;
  static constexpr idx max_idx = std::numeric_limits<idx>::max();
  reservation() : r(max_idx) {}
  idx get() const { return r.load();}
  bool reserve(idx i) { return parlay::write_min(&r, i, std::less<idx>());}
  bool reserved() const { return (r.load() < max_idx);}
  void reset() {r = max_idx;}
  bool check(idx i) const { return (r.load() == i);}
  bool check_reset(idx i) {
    if (r==i) { r = max_idx; return 1;}
    else return 0;
  }
};

enum status : char { done = 0, try_commit = 1, try_again = 2};

// Runs iterations from start to end in rounds of size approximately
// (end-start)/granularity (although adapts to contention).
// Each iterations in a round tries to reserve and commit during the round.
// If it fails on either it is carried forward to the next round.
// The reserve can return one of three values:
//   - done: do not try to commit and do not carry forward
//   - try_commit: try to commit and if commit returns false carry forward
//   - try_again: do not try to commit but carry forward
// Lower iterations will win over higher ones during a reservation.
// Completes when all iterations from start to end have completed.
template <class idx, class R, class C>
void speculative_for(idx start, idx end,
                     R reserve, C commit,
                     long start_size=1) {
  long current_round_size = std::max(start_size, 1l);
  parlay::sequence<idx> carry_forward;

  long number_done = start; // number of iterations done
  long number_keep = 0; // number of iterations to carry to next round

  while (number_done < end) {
    long size = std::min(current_round_size, end - number_done);

    // try to reserve 
    parlay::sequence<status> keep = parlay::tabulate(size, [&] (long i) {
      long j = (i < number_keep) ? carry_forward[i] : number_done + i;
      return reserve(j); });

    // try to commit
    parlay::sequence<idx> indices = parlay::tabulate(size, [&] (long i) -> idx {
      long j = (i < number_keep) ? carry_forward[i] : number_done + i;
      if (keep[i] == try_commit) keep[i] = (commit(j) ? done : try_again);
      return j;
    });

    // keep iterations that failed for the next round
    auto flags = parlay::delayed_map(keep, [] (status x) {return x != done;});
    carry_forward = parlay::pack(indices, flags);
    number_keep = carry_forward.size();
    number_done += size - number_keep;

    // if few need to be carried forward, then double the round size
    if (float(number_keep)/float(size) < .2)
      current_round_size = current_round_size * 2;
  }
}
