#pragma once

#ifndef _THREAD_HANDLER_HPP_
#define _THREAD_HANDLER_HPP_

#include "_worker.hpp"
#include "atomic_barrier.hpp"

#include <array>
#include <atomic>
#include <sched.h>

namespace adapt {
namespace __internal__ // anonymous namespace
{

class ThreadHandler {
private:
  bool stop;
  std::array<unsigned long, ADPT_MAX_THREADS> threads;
  bool in_master();
  void start_threads();
  void stop_threads();

public:
  unsigned long master;
  size_t num_threads;
  size_t alpha;
  size_t grain;
  std::atomic<int> counter;
  AtomicBarrier barrier;
  std::array<cpu_set_t, ADPT_MAX_THREADS> cpusets;
  std::array<WorkerInterface *, ADPT_MAX_THREADS> workers_array;

  ThreadHandler();
  ~ThreadHandler();
  void work(int my_id);
  static void *spawn_worker(void *ptr);

  friend void adapt::start_workers();
  friend void adapt::stop_workers();
};

// Instantiates the handler on beginning of program. Destroy at program exit.
extern ThreadHandler thread_handler;

} // namespace __internal__
} // namespace adapt

#endif