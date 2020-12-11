#pragma once

#include "../sequence.h"
#include "../primitives.h"
#include "get_time.h"

namespace parlay {
  
  template <class Range, class Comp>
  auto group_by_key_sorted(Range const &S, Comp const &less) {
    using KV = typename std::remove_reference<Range>::type::value_type;
    using K = typename KV::first_type;
    using V = typename KV::second_type;
    internal::timer t("group by", false);
    size_t n = S.size();
  
    sequence<KV> sorted;
    if constexpr(std::is_integral_v<K> &&
		 std::is_unsigned_v<K> &&
		 sizeof(KV) <= 16) {
      sorted = parlay::integer_sort(make_slice(S), [] (KV a) {return a.first;});
    } else {
      auto pair_less = [&] (auto const &a, auto const &b) {
	  return less(a.first, b.first);};
      sorted = parlay::stable_sort(make_slice(S), pair_less);
    }
    
    auto vals = sequence<V>::uninitialized(n);

    auto idx = block_delayed::filter(iota(n), [&] (size_t i) {
        assign_uninitialized(vals[i], sorted[i].second);
	return (i==0) || less(sorted[i-1].first, sorted[i].first);});
    t.next("pack index");
  
    auto r = tabulate(idx.size(), [&] (size_t i) {
        size_t start = idx[i];				    
	size_t end = ((i==idx.size()-1) ? n : idx[i+1]);
	return std::pair(std::move(sorted[idx[i]].first),
			 to_sequence(vals.cut(start, end)));
    });
    t.next("make pairs");
    return r;
  }

  template <class Range>
  auto group_by_key_sorted(Range const &S) {
    return group_by_key_sorted(S, std::less{});}

}
