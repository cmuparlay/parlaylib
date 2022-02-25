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

// Runs iterations from start to end in rounds of size approximately
// (end-start)/granularity (although adapts to contention).
// Each iterations in a round tries to reserve and commit during the round.
// If it fails on either it is carried forward to the next round.
// Lower iterations will will over higher ones during a reservation.
// Completes when all iterations from start to end have completed.
template <class idx, class R, class C>
void speculative_for(idx start, idx end, 
		     R reserve, C commit,
		     long granularity = 10) {
  long max_round_size = (end-start)/granularity+1;
  long current_round_size = max_round_size/4+1;
  parlay::sequence<idx> Ihold;

  long number_done = start; // number of iterations done
  long number_keep = 0; // number of iterations to carry to next round

  while (number_done < end) {
    long size = std::min(current_round_size, end - number_done);

    // try to reserve (keep is true if succeeded so far)
    parlay::sequence<bool> keep = parlay::tabulate(size, [&] (long i) {
      long j = (i < number_keep) ? Ihold[i] : number_done + i;
      return reserve(j); });

    // try to commit
    parlay::sequence<idx> I = parlay::tabulate(size, [&] (long i) -> idx {
      if (keep[i]) {
        long j = (i < number_keep) ? Ihold[i] : number_done + i;
        keep[i] = !commit(j);
        return j;
      } else return 0;});

    // keep iterations that failed for the next round
    Ihold = parlay::pack(I.head(size), keep.head(size));
    number_keep = Ihold.size();
    number_done += size - number_keep;

    // adjust round size based on number of failed attempts
    if (float(number_keep)/float(size) > .2) // shrink
      current_round_size
	= std::max({number_keep, current_round_size/2, max_round_size/64});
    else if (float(number_keep)/float(size) < .1) // grow
      current_round_size = std::min(current_round_size * 2, max_round_size);
  }
}
