#pragma once

#ifndef _ADAPTIVE_HPP_
#define _ADAPTIVE_HPP_

#include <algorithm>
#include <array>
#include <assert.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <pthread.h>
#include <sched.h>
#include <string.h> //memset
#include <sys/syscall.h>
#include <thread>
#include <unistd.h>
#include <vector>

// 2 big (1 ~ 2), 4 LITTLE (0, 3 ~ 5)
#ifdef NVIDIA_JETSON_TX2
#define IS_LITTLE(id) ((id == 0) || (id > 2))

// 4 big (4 ~ 7) 4 LITTLE (0 ~ 3)
#elif ODROID_XU3_LITE
#define IS_LITTLE(id) (id < 4)

// no big.LITTLE
#else
#define IS_LITTLE(id) (id < 2)
#endif

#define MIN(x, y) ((x < y) ? x : y)
#define MAX(x, y) ((x > y) ? x : y)

#define ADPT_MAX_THREADS 256

namespace adapt {

namespace __internal__ // anonymous namespace
{

extern size_t num_threads;
extern size_t alpha;
extern size_t grain_number;

// AtomicBarrier =======================================================================================================

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

// AtomicMutex =========================================================================================================

class AtomicMutex {
  std::atomic<bool> locked;

public:
  AtomicMutex();
  void lock();
  void unlock();
  bool try_lock();
};

class AbstractWorker {
public:
  virtual void work() = 0;
};

template <class Index>
class Worker : public AbstractWorker {
protected:
  const Index _id;
  const Index _nthr;
  Index _working_first;
  Index _working_last;
  bool _is_reduction = false;
  std::vector<Worker *> *_workers_array;

public:
  std::atomic<Index> first; // first iteration of sub-range
  std::atomic<Index> last;  // last (+1) iteration of sub-range
  Index copy_first;         // non-atomic copy of first
  Index seq_chunk;          // chunk/grain size
  Index min_steal;          // square root of su-range size
  Index half_range;         // half of range
  AtomicMutex lock;         // worker lock
  bool is_LITTLE;

  Worker(const size_t thr_id, const Index global_first, const Index global_last, std::vector<Worker *> *workers_array) :
      _id(thr_id), _nthr(num_threads), _workers_array(workers_array) {
    is_LITTLE = IS_LITTLE(_id);

#ifdef DISTRIB_BY_DEMAND // thread 0 starts with entire range
    first = (thr_id == 0) ? global_first : Index(0);
    last  = (thr_id == 0) ? global_last : Index(0);
#elif DISTRIB_BY_ALPHA // big cores have initial range ALPHA times bigger
    const Index v_nthr = (num_threads - num_big_cores) + (num_big_cores * alpha);
    const Index v_size = (is_LITTLE) ? 1 : alpha;
    Index v_id         = 0;
    for (Index i = 0; i < _id; i++) v_id += (IS_LITTLE(i) ? 1 : alpha);
    const Index chunk  = (global_last - global_first) / v_nthr; // size of initial sub-range (integer division)
    const Index remain = (global_last - global_first) % v_nthr; // remains of integer division
    first              = chunk * v_id + global_first + MIN(v_id, remain); // first iteration of sub-range
    // last (+1) iteration of sub-range
    last           = first + (chunk * v_size) + Index((v_id < remain) ? MIN((remain - v_id), v_size) : 0);
#else                  // divides initial range equally
    const Index chunk  = (global_last - global_first) / _nthr; // size of initial sub-range (integer division)
    const Index remain = (global_last - global_first) % _nthr; // remains of integer division
    first              = chunk * _id + global_first + MIN(Index(_id), remain); // first iteration of sub-range
    last               = first + chunk + static_cast<Index>(_id < remain);     // last (+1) iteration of sub-range
#endif
    recalc_internal();
  }

  virtual ~Worker() {}

protected:
  inline void recalc_internal() {
    _working_first = _working_last = copy_first = first;
    const Index m                               = last - copy_first; // original size of sub-range
#ifdef GRAIN_FRACTION
    seq_chunk = MAX(m / grain_number, Index(1));
#elif GRAIN_LOG
    seq_chunk      = MAX(Index(std::log2(m)), Index(1)); // chunk/grain size to serial extraction
#else
    seq_chunk          = grain_number;
#endif
#ifdef GRAIN_SMALLER_LITTLE
    if (is_LITTLE) seq_chunk = MAX(seq_chunk / alpha, Index(1)); // LITTLE cores have half of grain
#endif
    half_range = m >> 1;
    min_steal  = MAX(Index(std::sqrt(m)), Index(1)); // minimal size to steal
  }

  /*
   * Method: extract_seq
   * --------------------------
   *   Extracts sequential work from sub-range of current thread.
   *   The amount of sequential work to be extracted is ALHPA * log2(m), where m is the sub-range size.
   *   If a conflict is detected (a stealer steals a fraction of the work that would be extracted),
   *   it locks itself and tries to extract again.
   *
   *   returns : true if it could extract 1 or more iterations
   */
  bool extract_seq() {
    // const Index old_first = first;
#ifdef EXTRACT_THRESHOLD
    const Index chunk = ((last - _working_last) > (seq_chunk << 2)) ? seq_chunk : 1;
    _working_first    = MIN((copy_first + chunk), static_cast<Index>(last));
#else
    _working_first = MIN((copy_first + seq_chunk), static_cast<Index>(last));
#endif
    if (_working_first > _working_last) {
      first = _working_first;
      if (_working_first < last) {
        _working_last   = _working_first;
        const Index tmp = copy_first;
        copy_first      = _working_first;
        _working_first  = tmp;
        return true;
      }
      first = copy_first; // conflict
    }
    /* conflict detected: rollback and lock */
    lock.lock();
    _working_first = copy_first;
    if (_working_first < last) first = _working_last = copy_first = last;
    lock.unlock();
    return (_working_first < first);
  }

  /*
   * Method: extract_par
   * --------------------------
   *   Steals sequential work from a sub-range of another thread.
   *   The algorithm randomly selects a sub-range from another thread and do the tests:
   *     - If it can be locked
   *     - If it have a minimal amount of work to be stealed
   *   The minimal amount is half of the remaining work, and it cannot be lower than sqrt(m),
   *   where m is the original sub-range size.
   *
   *   returns : true if it could steal work from someone, false otherwise
   */
  bool extract_par() {
    size_t remaining = _nthr - 1;
    size_t i;

    first = std::numeric_limits<Index>::max();
    last  = std::numeric_limits<Index>::max();

    std::array<bool, ADPT_MAX_THREADS> _visited;
    _visited.fill(false);
    _visited[_id] = true;

    // While there are sub-ranges that are not inspected yet
    while (remaining) {
      for (i = rand() % _nthr; _visited[i]; i = (i + 1) % _nthr)
        ; // Advances to an unvisited victim/sub-range
      Worker &victim = *(_workers_array->at(i));

#ifdef STEAL_ONLY_FROM_LITTLE
      if (victim.is_LITTLE && (victim.last > victim.first)) {
#else
      if (victim.last > victim.first) {
#endif
        if (victim.lock.try_lock()) {
          const Index vic_last = victim.last;
          Index steal_size;
#ifdef STEAL_HALF
          do {
            steal_size = victim.half_range;
            victim.half_range >>= 1;
          } while (victim.first > (vic_last - steal_size) && steal_size);
#else // STEAL_HALF_REMAINING
          const Index remain = vic_last - victim.first;
          steal_size         = (remain > 0) ? MAX(remain >> 1, Index(1)) : 0;
#endif
#ifdef STEAL_SMALLER_LITTLE
          if (is_LITTLE) steal_size /= alpha; // cut steal by half
#endif
#ifdef STEAL_THRESHOLD_LITTLE
          if ((steal_size < victim.min_steal) && is_LITTLE) {
            victim.lock.unlock();
            --remaining;
            _visited[i] = true;
            continue; // cancel if stealer is LITTLE and steal size is too small
          }
#endif
          const Index new_last = vic_last - steal_size;
          if ((victim.last > new_last) && (victim.first <= new_last)) { // verify overflow
            victim.last = new_last;
            if (victim.first > victim.last) {
              /* rollback and abort */
              victim.last = vic_last;
            } else if (steal_size > 0) {
#ifdef REDUCE_GRAIN_ON_STEAL
              victim.seq_chunk = MAX(victim.seq_chunk >> 1, Index(1));
#endif
              victim.lock.unlock();
              last  = vic_last;
              first = new_last;
              recalc_internal();
              return true;
            }
          }
          victim.lock.unlock();
        } else
          continue; // if cannot lock
      }
      --remaining;
      _visited[i] = true;
    }
    return false;
  }
}; // namespace __internal__

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
        if (an_thr < num_threads) {
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

class ThreadHandler {
private:
  bool stop;
  std::array<pthread_t, ADPT_MAX_THREADS> threads;

public:
  pthread_t master;
  std::atomic<int> counter;
  AtomicBarrier barrier;
  std::array<cpu_set_t, ADPT_MAX_THREADS> cpusets;
  std::array<AbstractWorker *, ADPT_MAX_THREADS> absWorkers;

  ThreadHandler();
  ~ThreadHandler();
  void work(int my_id);
  static void *spawn_worker(void *ptr);
};

// Instantiates the handler on beginning of program. Destroy at program exit.
extern ThreadHandler thread_handler;

} // namespace __internal__

extern size_t get_num_threads();

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
  using forworker_t = __internal__::ForWorker<Index, Function>;
  using worker_t    = __internal__::Worker<Index>;
  std::vector<worker_t *> workers(__internal__::num_threads, nullptr); // Data for workers

  for (size_t i = 0; i < __internal__::num_threads; i++) {
    forworker_t *worker                        = new forworker_t(i, first, last, &workers, local_compute);
    workers[i]                                 = static_cast<worker_t *>(worker);
    __internal__::thread_handler.absWorkers[i] = static_cast<__internal__::AbstractWorker *>(worker);
  }

  // this thread work
  __internal__::thread_handler.work(0);
  __internal__::thread_handler.barrier.wait();

  for (size_t i = 0; i < __internal__::num_threads; i++) delete workers[i];
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
  using redworker_t = __internal__::ReductionWorker<Index, Function, Value, Reduction>;
  using worker_t    = __internal__::Worker<Index>;
  std::vector<worker_t *> workers(__internal__::num_threads, nullptr); // Data for workers

  for (size_t i = 0; i < __internal__::num_threads; i++) {
    redworker_t *worker = new redworker_t(i, first, last, identity, &workers, local_compute, reduction);
    workers[i]          = static_cast<worker_t *>(worker);
    __internal__::thread_handler.absWorkers[i] = static_cast<__internal__::AbstractWorker *>(worker);
  }

  // this thread work
  __internal__::thread_handler.work(0);
  __internal__::thread_handler.barrier.wait();

  Value reduction_value = static_cast<redworker_t *>(workers[0])->reduction_value;

  for (size_t i = 0; i < __internal__::num_threads; i++) delete workers[i];
  workers.clear();

  return reduction_value;
}

}; // namespace adapt

#endif