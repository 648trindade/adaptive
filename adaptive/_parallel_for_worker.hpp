#pragma once

#ifndef _PARALLEL_FOR_WORKER_
#define _PARALLEL_FOR_WORKER_

#include "_worker.hpp"

namespace adapt {
namespace __internal__ // anonymous namespace
{

template <class Index, class Function>
class ForWorker : public Worker<Index> {
  const Function &local_compute;

public:
  ForWorker(const size_t thr_id,
            const Index global_first,
            const Index global_last,
            std::vector<Worker<Index> *> *workers_array,
            const Function &_local_compute) :
      Worker<Index>(thr_id, global_first, global_last, workers_array),
      local_compute(_local_compute) {}

  virtual void work() override {
    while (true) {                // Iterates while there are work to be done
      while (this->extract_seq()) // Iterates while there are sequential work to be done
        local_compute(this->_working_first, this->_working_last);

      // Tries to steal work. If there are no work to steal, exits
      if (!this->extract_par()) return;
    }
  }
};

} // namespace __internal__
} // namespace adapt

#endif