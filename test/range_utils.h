// Useful reusable stuff for testing sorting functions

#ifndef PARLAY_TEST_SORTING_UTILS
#define PARLAY_TEST_SORTING_UTILS

#include <numeric>
#include <utility>

#include <parlay/range.h>

#include <parlay/internal/delayed/common.h>

namespace parlay {

static_assert(parlay::is_range_v<parlay::sequence<int>>);
static_assert(parlay::is_range_v<const parlay::sequence<int>>);

// ----------------------------------------------------------------------------
//   Pretend that a random-access range is only block-iterable (for testing)
// ----------------------------------------------------------------------------

// Wrap a random access range in an interface that makes it look like a plain
// block-iterable delayed range
template<typename UnderlyingRange>
struct block_iterable_wrapper_t :
    public internal::delayed::block_iterable_view_base<UnderlyingRange, block_iterable_wrapper_t<UnderlyingRange>> {
  using base = internal::delayed::block_iterable_view_base<UnderlyingRange, block_iterable_wrapper_t<UnderlyingRange>>;
  using base::base_view;

  using reference = range_reference_type_t<UnderlyingRange>;
  using value_type = range_value_type_t<UnderlyingRange>;

  static_assert(parlay::is_block_iterable_range_v<UnderlyingRange>);

  // We need to wrap the internal iterator which might be random-access and make
  // it look like a non-random access iterator, so that we force the block-iterable
  // versions of the functions to be used.

  template<bool Const>
  struct iterator_t {
   private:
    using parent_type = block_iterable_wrapper_t<UnderlyingRange>;
    using base_view_type = maybe_const_t<Const, std::remove_reference_t<UnderlyingRange>>;
    using base_iterator_type = range_iterator_type_t<base_view_type>;

   public:
    using iterator_category = std::forward_iterator_tag;
    using reference = range_reference_type_t<base_view_type>;
    using value_type = range_value_type_t<base_view_type>;
    using difference_type = std::ptrdiff_t;
    using pointer = void;

    decltype(auto) operator*() const { return *it; }

    iterator_t& operator++() {
      ++it;
      return *this;
    }
    iterator_t operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    friend bool operator==(const iterator_t& x, const iterator_t& y) { return x.it == y.it; }
    friend bool operator!=(const iterator_t& x, const iterator_t& y) { return x.it != y.it; }

    // Conversion from non-const iterator to const iterator
    /* implicit */ iterator_t(const iterator_t<false>& other) : it(other.it) {}

    iterator_t() : it{} {}

   private:
    friend parent_type;
    explicit iterator_t(base_iterator_type it_) : it(it_) {}

    base_iterator_type it;
  };

  using iterator = iterator_t<false>;
  using const_iterator = iterator_t<true>;

  template<typename U>
  block_iterable_wrapper_t(U&& v, std::true_type) : base(std::forward<U>(v)) {}

  [[nodiscard]] size_t size() const { return parlay::size(base_view()); }
  auto get_num_blocks() const { return internal::delayed::num_blocks(base_view()); }

  // Disable const overloads if the underlying range does not support const
#define PARLAY_NOCONST                                                            \
  template<typename UR = UnderlyingRange, typename = std::enable_if_t<            \
     parlay::is_range_v<std::add_const_t<std::remove_reference_t<UR>>>, UR>>

  auto get_begin_block(size_t i) { return iterator(internal::delayed::begin_block(base_view(), i)); }
  PARLAY_NOCONST auto get_begin_block(size_t i) const { return const_iterator(internal::delayed::begin_block(base_view(), i)); }

  auto get_end_block(size_t i) { return get_begin_block(i + 1); }
  PARLAY_NOCONST auto get_end_block(size_t i) const { return get_begin_block(i + 1); }

  auto begin() { return get_begin_block(0); }
  PARLAY_NOCONST auto begin() const { return get_begin_block(0); }

  auto end() { return get_begin_block(get_num_blocks()); }
  PARLAY_NOCONST auto end() const { return get_begin_block(get_num_blocks()); }

#undef PARLAY_NOCONST
};

template<typename T>
auto block_iterable_wrapper(T&& t) {
  return block_iterable_wrapper_t<T>(std::forward<T>(t), {});
}

static_assert(parlay::is_range_v<const parlay::sequence<int>>);
static_assert(parlay::is_range_v<const parlay::block_iterable_wrapper_t<parlay::sequence<int>>>);

static_assert(parlay::is_block_iterable_range_v<parlay::block_iterable_wrapper_t<parlay::sequence<int>>>);
static_assert(parlay::is_block_iterable_range_v<parlay::block_iterable_wrapper_t<parlay::sequence<int>&>>);
static_assert(parlay::is_block_iterable_range_v<const parlay::block_iterable_wrapper_t<const parlay::sequence<int>>>);
static_assert(parlay::is_block_iterable_range_v<const parlay::block_iterable_wrapper_t<const parlay::sequence<int>&>>);


struct NonConstRange {
  explicit NonConstRange(size_t n) : v(n) { std::iota(v.begin(), v.end(), 0); }
  auto begin() { return v.begin(); }
  auto end() { return v.end(); }
  auto size() { return v.size(); }
  std::vector<int> v;
};

static_assert(parlay::is_random_access_range_v<NonConstRange>);
static_assert(!parlay::is_range_v<const NonConstRange>);
static_assert(!parlay::is_random_access_range_v<const NonConstRange>);

}

#endif  // PARLAY_TEST_SORTING_UTILS
