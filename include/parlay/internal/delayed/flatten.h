#ifndef PARLAY_INTERNAL_DELAYED_FILTER_H_
#define PARLAY_INTERNAL_DELAYED_FILTER_H_

#include <type_traits>

#include "../../monoid.h"
#include "../../range.h"
#include "../../type_traits.h"
#include "../../utilities.h"

#include "../sequence_ops.h"

#include "common.h"
#include "map.h"

namespace parlay {
namespace internal {
namespace delayed {

template<typename UnderlyingView,
    std::enable_if_t<std::is_reference_v<range_reference_type_t<UnderlyingView>>, int> = 0>
struct block_delayed_flatten_t : public block_iterable_view_base<UnderlyingView, block_delayed_flatten_t<UnderlyingView>> {

  using base = block_iterable_view_base<UnderlyingView, block_delayed_flatten_t<UnderlyingView>>;
  using base::get_view;

  using underlying_block_iterator_type = decltype(begin_block(std::declval<UnderlyingView&>(), 0));
  using internal_iterator_type = range_iterator_type_t<range_reference_type_t<UnderlyingView>>;

  using value_type = range_value_type_t<range_reference_type_t<UnderlyingView>>;
  using reference = range_reference_type_t<range_reference_type_t<UnderlyingView>>;

  template<typename UV>
  block_delayed_flatten_t(UV&& v, int) : base(std::forward<UV>(v)) {
    // Extra int parameter is used to ensure that the template doesn't accidentally take
    // over the job of the copy constructor, since that would make an attempted copy
    // into a nested flatten instead.

    // Note that the input in general is a block-iterable sequence of ranges. The "flattening"
    // that we have to perform is therefore essentially a three-level flattening, since we have
    // to flatten the blocks in the input, and then the actual inner ranges themselves.
    //
    // Each element in the output is therefore be represented by three values: the index of the
    // original block that it occured in, an iterator to the element of the outer sequence that
    // it occured in, and an iterator to the element of the inner sequence, which points at the
    // output value.
    //
    //                               block_id
    //                                  |   external_it
    //                                  |       | internal_it
    //                                  |       |     |
    //                                  v       v     v
    //  [ "xxxxxxx", "xxxx", "xxxxx" ], [ "xx", "xxxxxXx", "xxxxxx", "x" ], [ "x", "xxxx" ]
    //
    // To further complicate things, the output is a block-iterable sequence!

    auto n_input_blocks = num_blocks(v);
    auto n_input_elements = parlay::size(v);

    // Compute "input offsets", which are, for each element in the input (outer)
    // sequence, the position of its first element in the output sequence
    auto input_offsets = parlay::internal::delayed::to_sequence(parlay::internal::delayed::map(v, [](const auto& r) { return parlay::size(r); }));
    n_elements = parlay::internal::scan_inplace(make_slice(input_offsets), addm<size_t>{});

    n_blocks = num_blocks_from_size(n_elements);
    block_starts = sequence<size_t>::uninitialized(n_blocks+1);
    external_starts = sequence<underlying_block_iterator_type>::uninitialized(n_blocks+1);
    internal_starts = sequence<internal_iterator_type>::uninitialized(n_blocks+1);

    // For the sentinel iterator at the end. We set external_starts[n_blocks] and internal_starts[n_blocks] later
    assign_uninitialized(block_starts[n_blocks], n_input_blocks);

    // Compute "output block offsets", which are, for each block in the output sequence,
    // the index of the element in the input sequence that contains the block's first element
    //
    // For convenience, we also compute block_starts here, which are the indices of the blocks
    // in the input sequence that contain the starting element for each block of the output
    auto out_block_offsets = parlay::internal::tabulate(n_blocks, [&](size_t i) -> size_t {
      size_t start = i * block_size;
      auto res = std::distance(std::begin(input_offsets), std::upper_bound(std::begin(input_offsets), std::end(input_offsets), start)) - 1;
      assign_uninitialized(block_starts[i], res / block_size);
      return res;
    });

    // For each block in the input sequence, iterate it (sequentially since we have to), and
    // find the locations at which the blocks of the output sequence begin.
    parallel_for(0, n_input_blocks, [&](size_t i) {
      size_t block_start = i * block_size, block_end = std::min((i + 1) * block_size, input_offsets.size());
      size_t t = std::distance(std::begin(out_block_offsets),
                       std::lower_bound(std::begin(out_block_offsets), std::end(out_block_offsets), block_start));

      size_t outer_idx = out_block_offsets[t];
      auto outer_it = begin_block(v, i);

      if (outer_idx < block_end) {
        std::advance(outer_it, outer_idx - block_start);

        for (; outer_it != end_block(v, i) && t < out_block_offsets.size() && out_block_offsets[t] < block_end;
               outer_it++, outer_idx++, t++) {
          for (; t < out_block_offsets.size() && out_block_offsets[t] == outer_idx; t++) {
            assign_uninitialized(external_starts[t], outer_it);
            auto inner_it = std::begin(*outer_it);
            std::advance(inner_it, input_offsets[outer_idx] - t * block_size);
            assign_uninitialized(internal_starts[t], inner_it);
          }

          // Write down the final element of the input since we use its end as the end sentinel
          if (outer_idx == n_input_elements - 1) {
            assign_uninitialized(external_starts[n_blocks], outer_it);
            assign_uninitialized(internal_starts[n_blocks], std::end(*outer_it));
          }
        }
      }

      // If we didn't find it earlier, write down the final element of
      // the input since we use its end as the end sentinel
      if (i == n_input_blocks - 1 && outer_idx < n_input_elements - 1) {
        std::advance(outer_it, (n_input_elements - 1) - outer_idx);
        assign_uninitialized(external_starts[n_blocks], outer_it);
        assign_uninitialized(internal_starts[n_blocks], std::end(*outer_it));
      }
    });
  }

  struct block_iterator {
    using iterator_category = std::forward_iterator_tag;
    using reference = reference;
    using value_type = value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    decltype(auto) operator*() const { return *inner_it; }

    block_iterator& operator++() {
      assert(block_id < num_blocks(*parent));
      ++inner_it;

      while (inner_it == std::end(*outer_it)) {          // while loop instead of if in case of empty inner sequences
        ++outer_it;
        while (outer_it == end_block(*parent, block_id)) {
          if (++block_id == num_blocks(*parent)) return *this;
          outer_it = begin_block(*parent, block_id);
        }
        inner_it = std::begin(*outer_it);
      }
      return *this;
    }


    block_iterator operator++(int) const { block_iterator ip = *this; ++ip; return ip; }

    bool operator==(const block_iterator& other) const { return inner_it == other.inner_it; }
    bool operator!=(const block_iterator& other) const { return inner_it != other.inner_it; }

   private:
    friend struct block_delayed_flatten_t<UnderlyingView>;
    block_iterator(size_t block_id_, underlying_block_iterator_type outer_it_, const internal_iterator_type inner_it_,
                   std::remove_reference_t<UnderlyingView>* parent_)
        : block_id(block_id_), outer_it(outer_it_), inner_it(inner_it_), parent(parent_) {}

    size_t block_id;
    underlying_block_iterator_type outer_it;
    internal_iterator_type inner_it;
    std::remove_reference_t<UnderlyingView>* parent;
  };

  // Returns the number of blocks
  auto get_num_blocks() const { return n_blocks; }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) const { return block_iterator(block_starts[i], external_starts[i], internal_starts[i], &get_view()); }

  // Return the sentinel denoting the end of block i
  auto get_end_block(size_t i) const { return block_iterator(block_starts[i+1], external_starts[i+1], internal_starts[i+1], &get_view()); }

  [[nodiscard]] size_t size() const { return n_elements; }

 private:
  size_t n_blocks, n_elements;
  sequence<size_t> block_starts;
  sequence<underlying_block_iterator_type> external_starts;
  sequence<internal_iterator_type> internal_starts;
};

template<typename UnderlyingView>
struct block_delayed_flatten_copy_t : public block_iterable_view_base<UnderlyingView, block_delayed_flatten_copy_t<UnderlyingView>> {


};

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_FILTER_H_
