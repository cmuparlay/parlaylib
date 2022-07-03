
#ifndef PARLAY_INTERNAL_CONCURRENCY_STICKY_COUNTER_H
#define PARLAY_INTERNAL_CONCURRENCY_STICKY_COUNTER_H

namespace parlay {
namespace internal {
namespace concurrency {

// A wait-free atomic counter that supports increment and decrement,
// such that attempting to increment the counter from zero fails and
// does not perform the increment.
//
// Useful for implementing reference counting, where the underlying
// managed memory is freed when the counter hits zero, so that other
// racing threads can not increment the counter back up from zero
//
// Assumption: The counter should never go negative. That is, the
// user should never decrement the counter by an amount greater
// than its current value
//
// Note: The counter steals the top two bits of the integer for book-
// keeping purposes. Hence the maximum representable value in the
// counter is 2^(8*sizeof(T)-2) - 1
template<typename T>
class sticky_counter {
  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>);
  
 public:

  [[nodiscard]] bool is_lock_free() const { return true; }
  static constexpr bool is_always_lock_free = true;
  [[nodiscard]] constexpr T max_value() const { return zero_pending_flag - 1; }

  sticky_counter() noexcept : x(1) {}
  explicit sticky_counter(T desired) noexcept : x(desired == 0 ? zero_flag : desired) {}

  // Increment the counter by the given amount if the counter is not zero.
  //
  // Returns true if the increment was successful, i.e., the counter
  // was not stuck at zero. Returns false if the counter was zero
  bool increment(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
    if (x.load() & zero_flag) return false;
    auto val = x.fetch_add(arg, order);
    return (val & zero_flag) == 0;
  }

  // Decrement the counter by the given amount. The counter must initially be
  // at least this amount, i.e., it is not permitted to decrement the counter
  // to a negative number.
  //
  // Returns true if the counter was decremented to zero. Returns
  // false if the counter was not decremented to zero
  bool decrement(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept {
    if (x.fetch_sub(arg, order) == arg) {
      T expected = 0;
      if (x.compare_exchange_strong(expected, zero_flag)) [[likely]] return true;
      else if ((expected & zero_pending_flag) && (x.exchange(zero_flag) & zero_pending_flag)) return true;
    }
    return false;
  }

  // Loads the current value of the counter. If the current value is zero, it is guaranteed
  // to remain zero until the counter is reset
  T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    auto val = x.load(order);
    if (val == 0 && x.compare_exchange_strong(val, zero_flag | zero_pending_flag)) [[unlikely]] return 0;
    return (val & zero_flag) ? 0 : val;
  }

  // Resets the value of the counter to the given value. This may be called when the counter
  // is zero to bring it back to a non-zero value.
  //
  // It is not permitted to race with an increment or decrement.
  void reset(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept {
    x.store(desired == 0 ? zero_flag : desired, order);
  }

 private:
  static constexpr inline T zero_flag = (T(1) << (sizeof(T)*8 - 1));
  static constexpr inline T zero_pending_flag = (T(1) << (sizeof(T)*8 - 2));

  mutable std::atomic<T> x;
};

}  // namespace concurrency
}  // namespace internal
}  // namespace parlay


#endif  // PARLAY_INTERNAL_CONCURRENCY_STICKY_COUNTER_H
