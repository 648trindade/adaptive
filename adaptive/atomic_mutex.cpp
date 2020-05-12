#include "atomic_mutex.hpp"

namespace adapt {

AtomicMutex::AtomicMutex() : locked(false) {}

void AtomicMutex::lock() {
  bool is_locked = false;
  while (!this->locked.compare_exchange_strong(is_locked, true, std::memory_order_release, std::memory_order_relaxed)) {
    is_locked = false;
  }
}

void AtomicMutex::unlock() { this->locked = false; }

bool AtomicMutex::try_lock() {
  bool is_locked = false;
  if (this->locked.compare_exchange_strong(is_locked, true, std::memory_order_release, std::memory_order_relaxed)) {
    return true;
  }
  return false;
}

} // namespace adapt