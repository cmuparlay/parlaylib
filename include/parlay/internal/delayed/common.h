#ifndef PARLAY_INTERNAL_DELAYED_COMMON_H
#define PARLAY_INTERNAL_DELAYED_COMMON_H

#include <cstddef>

#include <algorithm>
#include <functional>
#include <utility>
#include <type_traits>

#include "../../range.h"

namespace parlay {
namespace internal {
namespace delayed {

//  --------------------- Useful concept traits -----------------------

// Defines the member value true if, given a Range type Range_, and a unary operator
// UnaryOperator_ to be applied to each element of the range, the operator can be
// applied to the range even when it is const-qualified
//
// This trait is used to selectively disable const overloads of member functions
// (e.g., begin and end) when it would be ill-formed to attempt to transform
// the range when it is const.
template<typename Range_, typename UnaryOperator_, typename = std::void_t<>>
struct is_range_const_transformable : std::false_type {};

template<typename Range_, typename UnaryOperator_>
struct is_range_const_transformable<Range_, UnaryOperator_, std::void_t<
  std::enable_if_t< is_range_v<std::add_const_t<std::remove_reference_t<Range_>>> >,
  std::enable_if_t< std::is_invocable_v<const UnaryOperator_&, range_reference_type_t<std::add_const_t<std::remove_reference_t<Range_>>>> >
>> : std::true_type {};

// True if, given a Range type Range_, and a unary operator UnaryOperator_ to be
// applied to each element of the range, the operator can be applied to the range
// even when it is const-qualified
template<typename UV, typename UO>
static inline constexpr bool is_range_const_transformable_v = is_range_const_transformable<UV,UO>::value;

// Defines the member value true if the given type implements get_begin_block(size_t)
template<typename T, typename = std::void_t<>>
struct has_begin_block : public std::false_type {};

template<typename T>
struct has_begin_block<T, std::void_t<
  decltype( std::declval<T&>().get_begin_block(std::declval<size_t>()) )
>> : public std::true_type {};

// True if the given type implements get_begin_block(size_t)
template<typename T>
inline constexpr bool has_begin_block_v = has_begin_block<T>::value;

// Helper functional that dereferences an indirectly readable object
// (iterator/pointer/optional/etc) passed to it
struct dereference {
  template<typename IndirectlyReadable>
  decltype(auto) operator()(IndirectlyReadable&& it) const { return *std::forward<IndirectlyReadable>(it); }
};

// ----------------------------------------------------------------------------
//                        Block-iterable range parameters
// ----------------------------------------------------------------------------

// Default block size to use for block-iterable sequences
static constexpr size_t block_size = 2000;

inline constexpr size_t num_blocks_from_size(size_t n) {
  return (n == 0) ? 0 : (1 + (n - 1) / block_size);
}

// ----------------------------------------------------------------------------
//              Block-iterable interface for random-access ranges
// ----------------------------------------------------------------------------

template<typename Range,
         std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
size_t num_blocks(Range&& r) {
  auto n = parlay::size(r);
  return num_blocks_from_size(n);
}

template<typename Range,
         std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto begin_block(Range&& r, size_t i) {
  size_t n = parlay::size(r);

  // Note: For the interface requirements of a block-iterable sequence,
  // we require begin_block(n_blocks) to be valid and point to the end
  // iterator of the sequence, since this means that we always satisfy
  // end_block(r, i) == begin_block(r, i+1).
  size_t start = (std::min)(i * block_size, n);
  return std::begin(r) + start;
}

template<typename Range,
         std::enable_if_t<is_random_access_range_v<Range>, int> = 0>
auto end_block(Range&& r, size_t i) {
  size_t n = parlay::size(r);

  size_t end = (std::min)((i+1) * block_size, n);
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

// Given a view over which we want to perform a delayed operation, we either
// want to store a reference to it, or a copy. If the input view is provided
// as an lvalue reference, we will keep a reference, but we will do so inside
// a reference wrapper to ensure that it is copy-assignable.
//
// If the input view is a prvalue or rvalue reference, we take ownership of it
// and keep it as a value member.
template<typename T>
using view_storage_type = std::conditional_t<std::is_lvalue_reference_v<T>,
                            std::reference_wrapper<std::remove_reference_t<T>>,
                                                   std::remove_reference_t<T>>;

template<typename UnderlyingView>
struct block_iterable_view_base_data {
  explicit block_iterable_view_base_data(UnderlyingView&& v) : view_m(static_cast<UnderlyingView&&>(v)) { }

  UnderlyingView& base_view() { return view_m; }
  const UnderlyingView& base_view() const { return view_m; }

 private:
  view_storage_type<UnderlyingView> view_m;
};

template<>
struct block_iterable_view_base_data<void> { };

// This base class helps to implement block-iterable delayed views.
//
// The parent class must implement get_begin_block(size_t), which, given an index i,
// returns an iterator to the beginning of block i, and get_num_blocks(), which returns
// the number of blocks in the range. Note, importantly, that begin_begin_block(n)
// for n = get_num_blocks() should be valid and return an end iterator for the range.
//
// The base class will take care of implementing:
//
//  Method            |  Equivalent to
//  ----------------------------------------------------------
//  get_end_block(i)  |  get_begin_block(i+1)
//  begin()           |  get_begin_block(0)
//  end()             |  get_begin_block(get_num_blocks())
//
// If the parent class implements a const overload of get_begin_block, then const
// overloads of these methods will also be present. Else, they will be disabled.
//
// The base class also helps to store the base view. If the underlying (base) view is
// a reference type, then it is wrapped in a reference_wrapper so that the parent
// view type is still copy-assignable. If the underlying view is not a reference type,
// then it is just stored as a value member.
//
// If the underlying view is void, then no extra data member is stored. This is useful
// if the view is actually going to move/copy the underlying data and store it itself.
//
// Template arguments:
//  UnderlyingView: the type of the underlying view that is being transformed
//  Parent: the type of the parent view class (CRTP)
template<typename UnderlyingView, typename Parent>
struct block_iterable_view_base : public block_iterable_view_base_data<UnderlyingView> {
 public:

  // Returns an interator pointing to the end of block i
  auto get_end_block(size_t i) { return parent()->get_begin_block(i+1); }

  // Returns an interator pointing to the end of block i
  template<typename P = const Parent, std::enable_if_t<has_begin_block_v<P>, int> = 0>
  auto get_end_block(size_t i) const { return parent()->get_begin_block(i+1); }

  // Returns an iterator pointing to the first element of the range
  auto begin() { return parent()->get_begin_block(0); }

  // Returns an iterator pointing to the first element of the range
  template<typename P = const Parent, std::enable_if_t<has_begin_block_v<P>, int> = 0>
  auto begin() const { return parent()->get_begin_block(0); }

  // Returns an iterator pointing to the end of the range
  auto end() { return parent()->get_begin_block(parent()->get_num_blocks()); }

  // Returns an iterator pointing to the end of the range
  template<typename P = const Parent, std::enable_if_t<has_begin_block_v<P>, int> = 0>
  auto end() const { return parent()->get_begin_block(parent()->get_num_blocks()); }

 protected:
  block_iterable_view_base() = default;

  template<typename UV>
  explicit block_iterable_view_base(UV&& v, int)
    : block_iterable_view_base_data<UnderlyingView>(std::forward<UV>(v)) { }

 private:
  Parent* parent() { return static_cast<Parent*>(this); }
  const Parent* parent() const { return static_cast<const Parent*>(this); }
};


}  // namespace delayed
}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_DELAYED_COMMON_H
