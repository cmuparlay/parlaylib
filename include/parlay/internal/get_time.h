#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

namespace parlay {
  namespace internal {
struct timer {

private: 
  using time_t = decltype(std::chrono::system_clock::now());
  double total_so_far;
  time_t last;
  bool on;
  std::string name;

  auto get_time() {
    return std::chrono::system_clock::now();
  }

  void report(double time, std::string str) {
    std::ios::fmtflags cout_settings = std::cout.flags();
    std::cout.precision(4);
    std::cout << std::fixed;
    std::cout << name << ": ";
    if (str.length() > 0)
      std::cout << str << ": ";
    std::cout << time << std::endl;
    std::cout.flags(cout_settings);
  }

  double diff(time_t t1, time_t t2) {
    return std::chrono::duration_cast<std::chrono::microseconds>(t1 - t2).count() / 1000000.0;
  }
public:


  timer(std::string name = "Parlay time", bool start_ = true)
  : total_so_far(0.0), on(false), name(name) {
    if (start_) start();
  }
  
  void start () {
    on = true;
    last = get_time();
  }

  double stop () {
    on = false;
    double d = diff(get_time(),last);
    total_so_far += d;
    return d;
  }

  void reset() {
     total_so_far=0.0;
     on=0;
  }

  double next_time() {
    if (!on) return 0.0;
    time_t t = get_time();
    double td = diff(t, last);
    total_so_far += td;
    last = t;
    return td;
  }

  double total_time() {
    if (on) return total_so_far + diff(get_time(), last);
    else return total_so_far;
  }

  void next(std::string str) {
    if (on) report(next_time(), str);
  }

  void total() {
    report(total_time(),"total");
  }
};

  }
}
