#include "atomic_barrier.hpp"

namespace adapt {

AtomicBarrier::AtomicBarrier() : _counter(0), _participants(1) {}

AtomicBarrier::AtomicBarrier(int _participants) : _counter(0), _participants(_participants) {}

void AtomicBarrier::set_participants(size_t _participants) {
  this->_counter      = 0;
  this->_participants = _participants;
}

void AtomicBarrier::reset() { _counter = 0; }

void AtomicBarrier::wait() {
  size_t partial = this->_counter++;
  size_t end     = partial - (partial % this->_participants) + this->_participants;
  while (this->_counter < end)
    ;
}

bool AtomicBarrier::is_free() { return (this->_counter % this->_participants) == 0; }

}