#pragma once

#include <stdlib.h>
#include <sys/time.h>
#include <iomanip>
#include <iostream>
#include <string>

struct timer {
#ifdef ENABLE_PARLAY_TIMER
  double total_time;
  double last_time;
  bool on;
  std::string name;
  struct timezone tzp;
#endif

#ifdef ENABLE_PARLAY_TIMER
  timer(std::string name = "PBBS time", bool _start = true)
  : total_time(0.0), on(false), name(name), tzp({0,0}) {
    if (_start) start();
  }
#else
  timer(std::string = "",bool _start=true) {}
#endif
  
  double get_time() {
#ifdef ENABLE_PARLAY_TIMER
    timeval now;
    gettimeofday(&now, &tzp);
    return ((double) now.tv_sec) + ((double) now.tv_usec)/1000000.;
#else
    return 0;
#endif
  }

  void start () {
#ifdef ENABLE_PARLAY_TIMER
    on = 1;
    last_time = get_time();
#endif
  }

  double stop () {
#ifdef ENABLE_PARLAY_TIMER
    on = 0;
    double d = (get_time()-last_time);
    total_time += d;
    return d;
#else
    return 0;
#endif
  }

  void reset() {
#ifdef ENABLE_PARLAY_TIMER
     total_time=0.0;
     on=0;
#endif
  }

  double get_total() {
#ifdef ENABLE_PARLAY_TIMER
    if (on) return total_time + get_time() - last_time;
    else return total_time;
#else
    return 0;
#endif
  }

  double get_next() {
#ifdef ENABLE_PARLAY_TIMER
    if (!on) return 0.0;
    double t = get_time();
    double td = t - last_time;
    total_time += td;
    last_time = t;
    return td;
#else
    return 0;
#endif
  }

  void report(double time, std::string str) {
#ifdef ENABLE_PARLAY_TIMER
    std::ios::fmtflags cout_settings = std::cout.flags();
    std::cout.precision(4);
    std::cout << std::fixed;
    std::cout << name << ": ";
    if (str.length() > 0)
      std::cout << str << ": ";
    std::cout << time << std::endl;
    std::cout.flags(cout_settings);
#endif
  }

  void total() {
#ifdef ENABLE_PARLAY_TIMER
    report(get_total(),"total");
    total_time = 0.0;
#endif
  }

  void reportTotal(std::string str) {
#ifdef ENABLE_PARLAY_TIMER
    report(get_total(), str);
#endif
  }

  void next(std::string str) {
#ifdef ENABLE_PARLAY_TIMER
    if (on) report(get_next(), str);
#endif
  }
};

static timer _tm;
#define startTime() _tm.start();
#define nextTime(_string) _tm.next(_string);
