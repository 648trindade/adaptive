#pragma once

#ifndef _ADAPTIVE_HPP_
#define _ADAPTIVE_HPP_

#include "_parallel_for_worker.hpp"
#include "_reduction_worker.hpp"
#include "_thread_handler.hpp"

namespace adapt {

/*
 * Function: adapt::parallel_for
 * ---------------------------
 *
 *           first : beggining of loop
 *            last : end of loop
 *   local_compute : loop body
 */
template <class Function, class Index>
void parallel_for(Index first, Index last, Function local_compute) {
  using namespace __internal__;
  using forworker_t        = ForWorker<Index, Function>;
  using worker_t           = Worker<Index>;
  const size_t num_threads = get_num_threads();
  std::vector<worker_t *> workers(num_threads, nullptr); // Data for workers

  for (size_t i = 0; i < num_threads; i++) {
    forworker_t *worker          = new forworker_t(i, first, last, &workers, local_compute);
    workers[i]                   = static_cast<worker_t *>(worker);
    thread_handler.workers_array[i] = static_cast<WorkerInterface *>(worker);
  }

  // this thread work
  thread_handler.work(0);
  thread_handler.barrier.wait();

  for (size_t i = 0; i < num_threads; i++) delete workers[i];
  workers.clear();
}

/*
 * Function: adapt::parallel_reduce
 * ---------------------------
 *
 *           first : beggining of loop
 *            last : end of loop
 *        identity : intial value of reduction
 *   local_compute : loop body
 *       reduction : reduction function
 */
template <class Function, class Index, class Reduction, class Value>
Value parallel_reduce(const Index first,
                      const Index last,
                      const Value identity,
                      const Function &local_compute,
                      const Reduction &reduction) {
  using namespace __internal__;
  using redworker_t        = ReductionWorker<Index, Function, Value, Reduction>;
  using worker_t           = Worker<Index>;
  const size_t num_threads = get_num_threads();
  std::vector<worker_t *> workers(num_threads, nullptr); // Data for workers

  for (size_t i = 0; i < num_threads; i++) {
    redworker_t *worker          = new redworker_t(i, first, last, identity, &workers, local_compute, reduction);
    workers[i]                   = static_cast<worker_t *>(worker);
    thread_handler.workers_array[i] = static_cast<WorkerInterface *>(worker);
  }

  // this thread work
  thread_handler.work(0);
  thread_handler.barrier.wait();

  Value reduction_value = static_cast<redworker_t *>(workers[0])->reduction_value;

  for (size_t i = 0; i < num_threads; i++) delete workers[i];
  workers.clear();

  return reduction_value;
}

}; // namespace adapt

#endif