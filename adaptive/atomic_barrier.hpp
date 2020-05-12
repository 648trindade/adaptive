#pragma once

#ifndef _ATOMIC_BARRIER_
#define _ATOMIC_BARRIER_

#include <atomic>
#include <unistd.h>

namespace adapt {

class AtomicBarrier {
private:
  std::atomic<size_t> _counter;
  size_t _participants;

public:
  AtomicBarrier();
  AtomicBarrier(int _participants);
  void set_participants(size_t _participants);
  void reset();
  void wait();
  bool is_free();
};

} // namespace adapt

#endif