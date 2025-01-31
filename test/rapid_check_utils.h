// For automatically generating a parlay::sequence in rapidcheck

#ifndef PARLAY_RAPID_CHECK_UTILS
#define PARLAY_RAPID_CHECK_UTILS

#include <signal.h>

#include "rapidcheck.h"

#include "parlay/sequence.h"

namespace rc {

template<typename T>
struct Arbitrary<parlay::sequence<T>> {
  static Gen<parlay::sequence<T>> arbitrary() {
    auto r = gen::map<std::vector<T>>([](std::vector<T> x){
      return parlay::sequence<T>(x.begin(), x.end());
    });
    return r;
  }
};

template<typename T>
std::string container_to_string(T list) {
  std::ostringstream os;
  os << "[";
  for (size_t i = 0; i < list.size(); i++) {
    os << list[i];
    if (i != list.size() - 1) {
      os << ", ";
    }
  }
  os << "]";
  return os.str();
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const parlay::sequence<T>& seq) {
  using namespace parlay;
  os << container_to_string(seq);
  return os;
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& seq) {
  using namespace std;
  os << container_to_string(seq);
  return os;
}

std::string test_data;

void signal_handler(int signum) {
  std::cerr << "Signal " << signum << ": " << sigabbrev_np(signum) << '\n';
  std::cerr << test_data << '\n';
  exit(1);
}

/**
 * Stores the current test data as a string and print it if the program crashes with an irrecoverable signal. This
 * helps with debugging by showing the dataset that caused the crash.
 *
 * @tparam T
 */
template<typename T>
struct signal_interceptor {
  static constexpr int signals[] = {SIGFPE, SIGSEGV, SIGABRT, SIGILL, SIGBUS};

  explicit signal_interceptor(const T &data) {
    std::ostringstream os;
    os << data;
    test_data = os.str();
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    for (auto signal : signals) {
      sigaction(signal, &act, nullptr);
    }
  }

  ~signal_interceptor() {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;
    for (auto signal : signals) {
      sigaction(signal, &act, nullptr);
    }
  }
};

} // namespace rc

#endif  // PARLAY_RAPID_CHECK_UTILS
