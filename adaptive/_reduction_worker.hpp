#pragma once

#ifndef _REDUCTION_WORKER_
#define _REDUCTION_WORKER_

#include "_worker.hpp"

namespace adapt {
namespace __internal__ // anonymous namespace
{

template <class Index, class Function, class Value, class Reduction>
class ReductionWorker : public Worker<Index> {
  const Function &local_compute;
  const Reduction &reduction;
  const Value identity;

public:
  Value reduction_value;
  AtomicMutex red_lock;

  ReductionWorker(const size_t thr_id,
                  const Index global_first,
                  const Index global_last,
                  const Value _identity,
                  std::vector<Worker<Index> *> *workers_array,
                  const Function &_local_compute,
                  const Reduction &_reduction) :
      Worker<Index>(thr_id, global_first, global_last, workers_array),
      local_compute(_local_compute), reduction(_reduction), identity(_identity), reduction_value(_identity) {
    red_lock.lock();
  }

  virtual ~ReductionWorker() {}

  virtual void work() override {
    while (true) {                  // Iterates while there are work to be done
      while (this->extract_seq()) { // Iterates while there are sequential work to be done
        const Value partial_value = this->local_compute(this->_working_first, this->_working_last, identity);
        reduction_value           = reduction(reduction_value, partial_value);
      }

      // Tries to steal work. If there are no work to steal, exits
      if (!this->extract_par()) break;
    }

    // tree reduction
    for (int i = 2;; i <<= 1) {
      if ((this->_id % i) == 0) {
        const int an_thr = this->_id + (i >> 1);
        if (an_thr < this->_nthr) {
          ReductionWorker *an_worker = static_cast<ReductionWorker *>(this->_workers_array->at(an_thr));
          an_worker->red_lock.lock();
          reduction_value = reduction(reduction_value, an_worker->reduction_value);
          an_worker->red_lock.unlock();
          continue;
        }
      }
      break;
    }
    red_lock.unlock();
  }
}; // ReductionWorker

} // namespace __internal__
} // namespace adapt

#endif