
#ifndef PARLAY_INTERNAL_UNINITIALIZED_ITERATOR_H_
#define PARLAY_INTERNAL_UNINITIALIZED_ITERATOR_H_

#include <iterator>
#include <type_traits>

#include "../range.h"
#include "../type_traits.h"

namespace parlay {
namespace internal {

// Given a container of uninitialized<T>, you can wrap its iterators with
// uninitialized_iterator_adapter to get an iterator whose value type is T!
//
// The resulting iterator will have the same iterator category as Iterator.
template<typename Iterator>
class uninitialized_iterator_adapter {
 public:
  using iterator_category = parlay::iterator_category_t<Iterator>;
  using difference_type = parlay::iterator_difference_type_t<Iterator>;
  using value_type = decltype(std::declval<parlay::iterator_value_type_t<Iterator>>().value);
  using reference = std::add_lvalue_reference_t<value_type>;
  using pointer = std::add_pointer_t<value_type>;

  uninitialized_iterator_adapter(Iterator it_) : it(it_) {}

  reference operator*() const { return it->value; }

  pointer operator->() { return std::addressof(it->value); }

  uninitialized_iterator_adapter& operator++() {
    ++it;
    return *this;
  }

  friend void swap(uninitialized_iterator_adapter& left, uninitialized_iterator_adapter& right) noexcept {
    std::swap(left.it, right.it);
  }

  // ------------------------ Enabled if input iterator ------------------------

  template<typename It = Iterator>
  auto operator++(int) -> std::enable_if_t<parlay::is_input_iterator_v<It>, uninitialized_iterator_adapter> {
    auto tmp = *this;
    ++(*this);
    return tmp;
  }

  template<typename It = Iterator>
  auto operator==(const uninitialized_iterator_adapter& other) const
      -> std::enable_if_t<parlay::is_input_iterator_v<It>, bool> {
    return it == other.it;
  }

  template<typename It = Iterator>
  auto operator!=(const uninitialized_iterator_adapter& other) const
      -> std::enable_if_t<parlay::is_input_iterator_v<It>, bool> {
    return it != other.it;
  }

  // ------------------------ Enabled if forward iterator ------------------------

  // Can't SFINAE special member functions so this is close enough until C++20
  template<typename It = Iterator, std::enable_if_t<parlay::is_forward_iterator_v<It>, int> = 0>
  uninitialized_iterator_adapter() : it{} {}

  // ------------------------ Enabled if bidirectional iterator ------------------------

  template<typename It = Iterator>
  auto operator--() -> std::enable_if_t<parlay::is_bidirectional_iterator_v<It>, uninitialized_iterator_adapter&> {
    it--;
    return *this;
  }

  template<typename It = Iterator>
  auto operator--(int) -> std::enable_if_t<parlay::is_bidirectional_iterator_v<It>, uninitialized_iterator_adapter> {
    auto tmp = *this;
    --(*this);
    return tmp;
  }

  // ------------------------ Enabled if random-access iterator ------------------------

  template<typename It = Iterator>
  auto operator+=(difference_type diff)
      -> std::enable_if_t<parlay::is_bidirectional_iterator_v<It>, uninitialized_iterator_adapter&> {
    it += diff;
    return *this;
  }

  template<typename It = Iterator>
  auto operator+(difference_type diff) const
      -> std::enable_if_t<parlay::is_bidirectional_iterator_v<It>, uninitialized_iterator_adapter> {
    auto result = *this;
    result += diff;
    return result;
  }

  template<typename It = Iterator>
  auto operator-=(difference_type diff)
      -> std::enable_if_t<parlay::is_bidirectional_iterator_v<It>, uninitialized_iterator_adapter&> {
    it -= diff;
    return *this;
  }

  template<typename It = Iterator>
  auto operator-(difference_type diff) const
      -> std::enable_if_t<parlay::is_bidirectional_iterator_v<It>, uninitialized_iterator_adapter> {
    auto result = *this;
    result -= diff;
    return result;
  }

  template<typename It = Iterator>
  auto operator-(const uninitialized_iterator_adapter& other) const
      -> std::enable_if_t<parlay::is_bidirectional_iterator_v<It>, difference_type> {
    return it - other.it;
  }

  template<typename It = Iterator>
  auto operator[](std::size_t p) const -> std::enable_if_t<parlay::is_random_access_iterator_v<It>, reference> {
    return it[p].value;
  }

  template<typename It = Iterator>
  auto operator<(const uninitialized_iterator_adapter& other) const
      -> std::enable_if_t<parlay::is_random_access_iterator_v<It>, bool> {
    return it < other.it;
  }

  template<typename It = Iterator>
  auto operator<=(const uninitialized_iterator_adapter& other) const
      -> std::enable_if_t<parlay::is_random_access_iterator_v<It>, bool> {
    return it <= other.it;
  }

  template<typename It = Iterator>
  auto operator>(const uninitialized_iterator_adapter& other) const
      -> std::enable_if_t<parlay::is_random_access_iterator_v<It>, bool> {
    return it > other.it;
  }

  template<typename It = Iterator>
  auto operator>=(const uninitialized_iterator_adapter& other) const
      -> std::enable_if_t<parlay::is_random_access_iterator_v<It>, bool> {
    return it >= other.it;
  }

 private:
  Iterator it;
};

}  // namespace internal
}  // namespace parlay

#endif  // PARLAY_INTERNAL_UNINITIALIZED_ITERATOR_H_
