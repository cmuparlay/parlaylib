#ifndef PARLAY_INTERNAL_DELAYED_COMMON_H
#define PARLAY_INTERNAL_DELAYED_COMMON_H

#include <iterator>
#include <type_traits>

#include "../../range.h"
#include "../../type_traits.h"

namespace parlay {
namespace internal {
namespace delayed {

// Default block size to use for block-iterable sequences
static constexpr size_t block_size = 2000;

// ----------------------------------------------------------------------------
//            Block-iterable interface for random-access ranges
// ----------------------------------------------------------------------------

size_t num_blocks_from_size(size_t n) {
  return 1 + (n - 1) / block_size;
}

template<typename Range,
         std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
size_t num_blocks(Range&& r) {
  auto n = parlay::size(r);
  return num_blocks_from_size(n);
}

template<typename Range,
         std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto begin_block(Range&& r, size_t i) {
  auto start = i * block_size;
  return std::begin(r) + start;
}

template<typename Range,
         std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto end_block(Range&& r, size_t i) {
  size_t n = parlay::size(r);
  size_t start = i * block_size;
  size_t end = std::min(start + block_size, n);
  return std::begin(r) + end;
}

// ----------------------------------------------------------------------------
//            Block-iterable interface for non-random-access ranges
// ----------------------------------------------------------------------------

template<typename Range,
         std::enable_if_t<!is_random_access_range_v<Range> && is_block_iterable_range_v<Range>, int> = 0>
size_t num_blocks(Range&& r) {
  return r.get_num_blocks();
}

template<typename Range,
         std::enable_if_t<!is_random_access_range_v<Range> && is_block_iterable_range_v<Range>, int> = 0>
auto begin_block(Range&& r, size_t i) {
  return r.get_begin_block(i);
}

template<typename Range,
         std::enable_if_t<!is_random_access_range_v<Range> && is_block_iterable_range_v<Range>, int> = 0>
auto end_block(Range&& r, size_t i) {
  return r.get_end_block(i);
}


// ----------------------------------------------------------------------------
//                          Base class for BID views
// ----------------------------------------------------------------------------

// This base class helps to implement block-iterable delayed views.
//
// If the underlying view is a reference type, then it is wrapped in a reference_wrapper
// so that the resulting view type is still copy-assignable. If the underlying view is
// not a reference type, then it is just stored as a value member as usual.
//
// It also implements a forward iterator that iterates over the blocks sequentially,
// making it a valid forward range. This means it can be used in range-based for loops.
//
// Template arguments:
//  UnderlyingView: the type of the underlying view that is being transformed
//  ParentBidView: the type of the parent view class (i.e., the class that is deriving from this one - CRTP style)
template<typename UnderlyingView, typename ParentBidView>
struct block_iterable_view_base {
 public:
  struct iterator {
   private:
    using block_iterator = decltype(std::declval<ParentBidView&>().get_begin_block(0));

   public:
    using iterator_category = std::forward_iterator_tag;
    using reference = typename std::iterator_traits<block_iterator>::reference;
    using value_type = typename std::iterator_traits<block_iterator>::value_type;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    iterator(const ParentBidView* _parent, size_t _block_id, block_iterator _it) :
        parent(_parent), block_id(_block_id), it(_it) { }

    decltype(auto) operator*() const { return *it; }

    iterator& operator++() {
      assert(block_id < parent->get_num_blocks());
      ++it;
      while (it == parent->get_end_block(block_id)) {
        block_id++;
        it = parent->get_begin_block(block_id);
      }
      return *this;
    }

    iterator operator++(int) { iterator i = *this; ++i; return i; }

    bool operator==(const iterator& other) const { return block_id == other.block_id && it == other.it; }
    bool operator!=(const iterator& other) const { return block_id != other.block_id || it != other.it; }

   private:
    const ParentBidView* parent;
    size_t block_id;
    block_iterator it;
  };

  auto begin() const { return iterator(parent(), 0, parent()->get_begin_block(0)); }
  auto end() const { return iterator(parent(), parent()->get_num_blocks(), parent()->get_begin_block(parent()->get_num_blocks())); }

 protected:
  explicit block_iterable_view_base(UnderlyingView&& v) : view_m(static_cast<UnderlyingView&&>(v)) { }

  UnderlyingView& get_view() { return view_m; }
  const UnderlyingView& get_view() const { return view_m; }

 private:
  using view_member = std::conditional_t<std::is_lvalue_reference_v<UnderlyingView>,
                                         std::reference_wrapper<std::remove_reference_t<UnderlyingView>>,
                                         UnderlyingView>;

  ParentBidView* parent() { return static_cast<ParentBidView*>(this); }
  const ParentBidView* parent() const { return static_cast<const ParentBidView*>(this); }

  view_member view_m;
};

// ----------------------------------------------------------------------------
//            Conversion of block-iterables to regular sequences
// ----------------------------------------------------------------------------

template<typename UnderlyingView,
    std::enable_if_t<is_random_access_range_v<UnderlyingView>, int> = 0>
auto to_sequence(UnderlyingView&& v) {
  return parlay::to_sequence(std::forward<UnderlyingView>(v));
}

template<typename UnderlyingView,
    std::enable_if_t<!is_random_access_range_v<UnderlyingView> &&
    parlay::is_block_iterable_range_v<UnderlyingView>, int> = 0>
auto to_sequence(UnderlyingView&& v) {
  auto sz = parlay::size(v);
  auto out = parlay::sequence<range_value_type_t<UnderlyingView>>::uninitialized(sz);
  parallel_for(0, parlay::internal::delayed::num_blocks(v), [&](size_t i) {
    std::uninitialized_copy(parlay::internal::delayed::begin_block(v, i), parlay::internal::delayed::end_block(v, i),
                            out.begin() + i * parlay::internal::delayed::block_size);
  });
  return out;
}

// ----------------------------------------------------------------------------
//   Pretend that a random-access range is only block-iterable (for testing)
// ----------------------------------------------------------------------------

// Wrap a random access range in an interface that makes it look like a plain
// block-iterable delayed range
template<typename UnderlyingRange>
struct block_iterable_wrapper_t : public block_iterable_view_base<UnderlyingRange, block_iterable_wrapper_t<UnderlyingRange>> {
  using base = block_iterable_view_base<UnderlyingRange, block_iterable_wrapper_t<UnderlyingRange>>;
  using base::get_view;

  using reference = range_reference_type_t<UnderlyingRange>;
  using value_type = range_value_type_t<UnderlyingRange>;

  template<typename U>
  block_iterable_wrapper_t(U&& v, std::true_type) : base(std::forward<U>(v)) {}

  [[nodiscard]] size_t size() const { return parlay::size(get_view()); }
  auto get_num_blocks() const { return num_blocks(get_view()); }
  auto get_begin_block(size_t i) const { return begin_block(get_view(), i); }
  auto get_end_block(size_t i) const { return end_block(get_view(), i); }
};

template<typename T>
auto block_iterable_wrapper(T&& t) {
  return block_iterable_wrapper_t<T>(std::forward<T>(t), {});
}

}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_COMMON_H