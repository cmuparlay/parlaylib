#ifndef PARLAY_INTERNAL_DELAYED_FLATTEN_H_
#define PARLAY_INTERNAL_DELAYED_FLATTEN_H_

#include <cstddef>

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>

#include "../../monoid.h"
#include "../../parallel.h"
#include "../../range.h"
#include "../../sequence.h"
#include "../../slice.h"
#include "../../type_traits.h"
#include "../../utilities.h"

#include "../sequence_ops.h"

#include "common.h"
#include "map.h"
#include "terminal.h"

namespace parlay {
namespace internal {
namespace delayed {

template<typename UnderlyingView>
struct block_delayed_flatten_t :
    public block_iterable_view_base<UnderlyingView, block_delayed_flatten_t<UnderlyingView>> {

 private:
  static_assert(is_block_iterable_range_v<UnderlyingView>);
  static_assert(is_forward_range_v<range_reference_type_t<UnderlyingView>>);
  static_assert(std::is_reference_v<range_reference_type_t<UnderlyingView>>);

  using base = block_iterable_view_base<UnderlyingView, block_delayed_flatten_t<UnderlyingView>>;
  using base::base_view;

  using outer_iterator_type = range_iterator_type_t<UnderlyingView>;
  using inner_iterator_type = range_iterator_type_t<range_reference_type_t<UnderlyingView>>;

 public:
  using reference = range_reference_type_t<range_reference_type_t<UnderlyingView>>;
  using value_type = range_value_type_t<range_reference_type_t<UnderlyingView>>;

  template<typename UV>
  block_delayed_flatten_t(UV&& v, int) : base(std::forward<UV>(v), 0) {
    //                            ^
    // Extra int parameter is used to ensure that this template doesn't
    // accidentally take over the job of the copy constructor.
    initialize_iterators();
  }

  // The default copy constructor is not viable because it might copy dangling iterators
  block_delayed_flatten_t(const block_delayed_flatten_t& other) : base(other) {
    initialize_iterators();
  }

  block_delayed_flatten_t(block_delayed_flatten_t&&) noexcept(
    std::is_nothrow_move_constructible_v<base>                          &&
    std::is_nothrow_move_constructible_v<sequence<outer_iterator_type>> &&
    std::is_nothrow_move_constructible_v<sequence<inner_iterator_type>>) = default;

  friend void swap(block_delayed_flatten_t& first, block_delayed_flatten_t& second) {
    using std::swap;
    swap(first.base_view(), second.base_view());
    swap(first.n_blocks, second.n_blocks);
    swap(first.n_elements, second.n_elements);
    swap(first.outer_starts, second.outer_starts);
    swap(first.inner_starts, second.inner_starts);
  }

  block_delayed_flatten_t& operator=(block_delayed_flatten_t other) {
    swap(*this, other);
    return *this;
  }

  template<bool Const>
  struct iterator_t {
   private:
    using parent_type = maybe_const_t<Const, block_delayed_flatten_t<UnderlyingView>>;
    using base_view_type = maybe_const_t<Const, std::remove_reference_t<UnderlyingView>>;
    using outer_iterator_type = range_iterator_type_t<base_view_type>;
    using inner_iterator_type = range_iterator_type_t<range_reference_type_t<base_view_type>>;

    static_assert(std::is_same_v<inner_iterator_type,
                                 range_iterator_type_t<decltype(*std::declval<outer_iterator_type&>())>>);

   public:
    using iterator_category = std::forward_iterator_tag;
    using reference = range_reference_type_t<range_reference_type_t<base_view_type>>;
    using value_type = range_value_type_t<range_reference_type_t<base_view_type>>;;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    decltype(auto) operator*() const { return *inner_it; }

    iterator_t& operator++() {
      ++inner_it;

      while (inner_it == std::end(*outer_it)) {          // while loop instead of if in case of empty inner sequences
        if (++outer_it == end_base) {
          inner_it = {};
          return *this;
        }
        inner_it = std::begin(*outer_it);
      }
      return *this;
    }

    iterator_t operator++(int) { auto tmp = *this; ++(*this); return tmp; }

    friend bool operator==(const iterator_t& x, const iterator_t& y) {
      return x.outer_it == y.outer_it && x.inner_it == y.inner_it;
    }

    friend bool operator!=(const iterator_t& x, const iterator_t& y) {
      return x.outer_it != y.outer_it || x.inner_it != y.inner_it;
    }

    // Conversion from non-const iterator to const iterator
    /* implicit */ iterator_t(const iterator_t<false>& other)
        : outer_it(other.outer_it), inner_it(other.inner_it), end_base(other.end_base) {}

    iterator_t() : outer_it{}, inner_it{}, end_base{} {}

   private:
    friend parent_type;

    iterator_t(outer_iterator_type outer_it_, inner_iterator_type inner_it_, outer_iterator_type end_base_)
        : outer_it(std::move(outer_it_)), inner_it(std::move(inner_it_)), end_base(end_base_) {}

    outer_iterator_type outer_it;
    inner_iterator_type inner_it;
    outer_iterator_type end_base;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  // Returns the number of elements in the flattened range
  [[nodiscard]] size_t size() const { return n_elements; }

  // Returns the number of blocks in the flattened range
  auto get_num_blocks() const { return n_blocks; }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) { return iterator(outer_starts[i], inner_starts[i], std::end(base_view())); }

  // Return an iterator pointing to the beginning of block i
  template<typename UV = const std::remove_reference_t<UnderlyingView>, std::enable_if_t<is_range_v<UV>, int> = 0>
  auto get_begin_block(size_t i) const { return const_iterator(outer_starts[i], inner_starts[i], std::end(base_view())); }

 private:
  void initialize_iterators() {
    // Compute "input offsets", which are, for each element in the input (outer)
    // sequence, the position of its first element in the output sequence
    auto offsets = parlay::internal::delayed::to_sequence(
        parlay::internal::delayed::map(base_view(), [](auto&& r) -> size_t { return parlay::size(r); }));
    n_elements = parlay::internal::scan_inplace(make_slice(offsets), plus<size_t>{});

    n_blocks = num_blocks_from_size(n_elements);
    outer_starts = sequence<outer_iterator_type>::uninitialized(n_blocks+1);
    inner_starts = sequence<inner_iterator_type>::uninitialized(n_blocks+1);

    // The random-access version of the algorithm can just binary
    // search the offsets for each block which is very cheap
    if constexpr (is_random_access_range_v<UnderlyingView>) {
      parlay::parallel_for(0, n_blocks, [&](size_t i) {
        size_t start = i * block_size;
        auto j = std::distance(std::begin(offsets),
                               std::upper_bound(std::begin(offsets), std::end(offsets), start)) - 1;
        auto external_it = std::begin(base_view()) + j;
        auto internal_it = std::begin(*external_it);
        std::advance(internal_it, (start - offsets[j]));
        assign_uninitialized(outer_starts[i], external_it);
        assign_uninitialized(inner_starts[i], internal_it);
      });
    }

    // The block-iterable version of the algorithm can not binary search the offsets,
    // so instead it linearly searches them within each block. To be efficient, it
    // does not do a new linear search for each element since this could take up to
    // quadratic work, but instead it can start from where the previous search ended
    else if (n_elements > 0) {

      // Compute "output block offsets", which are, for each block in the output sequence,
      // the index of the element in the input sequence that contains the block's first element
      auto out_block_offsets = parlay::internal::tabulate(n_blocks, [&](size_t i) -> size_t {
        size_t start = i * block_size;
        return std::distance(std::begin(offsets),
                             std::upper_bound(std::begin(offsets), std::end(offsets), start)) - 1;
      });

      // For each block in the input sequence, iterate it (sequentially since we have to), and
      // find the locations at which the blocks of the output sequence begin.
      parallel_for(0, num_blocks(base_view()), [&](size_t i) {
        size_t block_start = i * block_size, block_end = (std::min)((i + 1) * block_size, offsets.size());
        size_t out_block_id = std::distance(std::begin(out_block_offsets),
                                            std::lower_bound(std::begin(out_block_offsets), std::end(out_block_offsets), block_start));

        size_t outer_idx = out_block_offsets[out_block_id];
        if (outer_idx < block_end) {
          auto outer_it = begin_block(base_view(), i);
          std::advance(outer_it, outer_idx - block_start);

          auto outer_end = end_block(base_view(), i);
          for (; outer_it != outer_end && out_block_id < out_block_offsets.size() &&
                 out_block_offsets[out_block_id] < block_end; ++outer_it, ++outer_idx) {
            for (; out_block_id < out_block_offsets.size() && out_block_offsets[out_block_id] == outer_idx;
                 ++out_block_id) {
              auto inner_it = std::begin(*outer_it);
              std::advance(inner_it, out_block_id * block_size - offsets[outer_idx]);
              assign_uninitialized(outer_starts[out_block_id], outer_it);
              assign_uninitialized(inner_starts[out_block_id], inner_it);
            }
          }
        }
      });
    }

    // Sentinel for the end iterator. The inner sentinel is value-initialized, which
    // is safe because it will never be compared against any other iterator unless
    // outer_it = end(base_view()), in which case, the other is also value-initialized
    assign_uninitialized(outer_starts[n_blocks], std::end(base_view()));
    assign_uninitialized(inner_starts[n_blocks], {});
  }

  size_t n_blocks, n_elements;
  sequence<outer_iterator_type> outer_starts;
  sequence<inner_iterator_type> inner_starts;
};

// If we want to flatten a range of temporaries, we can not use the previous
// algorithms because they keeps references into the ranges, which will dangle.
//
// This version of flatten therefore just eagerly copies the sequence and
// then wraps it around the reference version of flatten.
template<typename UnderlyingView>
struct block_delayed_flatten_copy_t : public block_iterable_view_base<void, block_delayed_flatten_copy_t<UnderlyingView>> {
 private:
  using base = block_iterable_view_base<void, block_delayed_flatten_copy_t<UnderlyingView>>;

  using inner_sequence_type = range_value_type_t<UnderlyingView>;
  using flattener_type = block_delayed_flatten_t<sequence<inner_sequence_type>&>;

 public:
  using value_type = range_value_type_t<range_value_type_t<UnderlyingView>>;
  using reference = range_reference_type_t<range_value_type_t<UnderlyingView>>;

  using iterator = typename flattener_type::iterator;
  using const_iterator = typename flattener_type::const_iterator;

  template<typename UV>
  block_delayed_flatten_copy_t(UV&& v, int) : base(),
      data(parlay::internal::delayed::to_sequence(std::forward<UV>(v))), result(data, 0) {

  }

  // Returns the number of elements in the flattened range
  [[nodiscard]] size_t size() const { return result.size(); }

  // Returns the number of blocks
  auto get_num_blocks() const { return result.get_num_blocks(); }

  // Return an iterator pointing to the beginning of block i
  auto get_begin_block(size_t i) { return result.get_begin_block(i); }

  // Return an iterator pointing to the beginning of block i
  template<typename UV = const std::remove_reference_t<UnderlyingView>, std::enable_if_t<is_range_v<UV>, int> = 0>
  auto get_begin_block(size_t i) const { return result.get_begin_block(i); }

 private:
  sequence<inner_sequence_type> data;
  flattener_type result;
};

template<typename UnderlyingView,
    std::enable_if_t<std::is_reference_v<range_reference_type_t<UnderlyingView>>, int> = 0>
auto flatten(UnderlyingView&& v) {
  return block_delayed_flatten_t<UnderlyingView>(std::forward<UnderlyingView>(v), 0);
}

template<typename UnderlyingView,
    std::enable_if_t<!std::is_reference_v<range_reference_type_t<UnderlyingView>>, int> = 0>
auto flatten(UnderlyingView&& v) {
  return block_delayed_flatten_copy_t<UnderlyingView>(std::forward<UnderlyingView>(v), 0);
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_FLATTEN_H_
