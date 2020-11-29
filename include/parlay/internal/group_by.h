#pragma once

#include "../sequence.h"
#include "../primitives.h"
#include "get_time.h"

namespace parlay {
  
  template <class Range, class Comp>
  auto group_by_key(Range const &S, Comp const &less) {
    using KV = typename std::remove_reference<Range>::type::value_type;
    using K = typename KV::first_type;
    using V = typename KV::second_type;
    internal::timer t("group by", false);
    size_t n = S.size();
  
    auto pair_less = [&] (auto const &a, auto const &b) {
      return less(a.first, b.first);};

    auto sorted = parlay::stable_sort(std::move(S), pair_less);
    t.next("sort");
      
    auto idx = block_delayed::filter(iota(n), [&] (size_t i) {
	return (i==0) || pair_less(sorted[i-1], sorted[i]);});
    t.next("pack index");
  
    auto r = tabulate(idx.size(), [&] (size_t i) {
        size_t start = idx[i];				    
	size_t end = ((i==idx.size()-1) ? n : idx[i+1]);
	return std::pair(std::move(sorted[idx[i]].first),
			 to_sequence(sorted.cut(start, end)));
    });
    t.next("make pairs");
    return r;
  }

  template <class Range>
  auto group_by_key(Range const &S) {
    return group_by_key(S, std::less{});}

}
