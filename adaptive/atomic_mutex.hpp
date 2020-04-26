#pragma once

#ifndef _ATOMIC_MUTEX_
#define _ATOMIC_MUTEX_

#include <atomic>

namespace adapt {

class AtomicMutex {
  std::atomic<bool> locked;

public:
  AtomicMutex();
  void lock();
  void unlock();
  bool try_lock();
};

} // namespace adapt

#endif