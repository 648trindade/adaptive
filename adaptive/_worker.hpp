#pragma once

#ifndef _WORKER_HPP_
#define _WORKER_HPP_

#include "_defines.hpp"
#include "atomic_mutex.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace adapt {
namespace __internal__ // anonymous namespace
{

class WorkerInterface {
public:
  virtual void work() = 0;
};

template <class Index>
class Worker : public WorkerInterface {
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
      _id(thr_id), _nthr(get_num_threads()), _workers_array(workers_array) {
    this->is_LITTLE = IS_LITTLE(_id);

#ifdef DISTRIB_BY_DEMAND // thread 0 starts with entire range
    this->first = (this->_id == 0) ? global_first : Index(0);
    this->last  = (this->_id == 0) ? global_last : Index(0);
#elif DISTRIB_BY_ALPHA // big cores have initial range ALPHA times bigger
    const size_t alpha = get_alpha();
    const Index v_nthr = (this->_nthr - num_big_cores) + (num_big_cores * alpha);
    const Index v_size = (this->is_LITTLE) ? 1 : alpha;
    Index v_id         = 0;
    for (Index i = 0; i < _id; i++) v_id += (IS_LITTLE(i) ? 1 : alpha);
    const Index chunk    = (global_last - global_first) / v_nthr; // size of initial sub-range (integer division)
    const Index remain   = (global_last - global_first) % v_nthr; // remains of integer division
    this->first          = chunk * v_id + global_first + MIN(v_id, remain); // first iteration of sub-range
    this->last           = this->first + (chunk * v_size) + Index((v_id < remain) ? MIN((remain - v_id), v_size) : 0);
#else                  // divides initial range equally
    const Index chunk  = (global_last - global_first) / this->_nthr; // size of initial sub-range (integer division)
    const Index remain = (global_last - global_first) % this->_nthr; // remains of integer division
    this->first     = chunk * this->_id + global_first + MIN(Index(this->_id), remain); // first iteration of sub-range
    this->last      = this->first + chunk + static_cast<Index>(this->_id < remain); // last (+1) iteration of sub-range
#endif
    this->recalc_internal();
  }

  virtual ~Worker() {}

protected:
  inline void recalc_internal() {
    this->_working_first = this->_working_last = this->copy_first = this->first;
    const Index range_size = this->last - this->copy_first; // original size of sub-range
    const size_t grain     = get_grain();
#ifdef GRAIN_FRACTION
    this->seq_chunk = MAX(range_size / grain, Index(1));
#elif GRAIN_LOG
    this->seq_chunk      = MAX(Index(std::log2(range_size)), Index(1)); // chunk/grain size to serial extraction
#else
    this->seq_chunk = grain;
#endif
#ifdef GRAIN_SMALLER_LITTLE
    if (this->is_LITTLE)
      this->seq_chunk = MAX(this->seq_chunk / get_alpha(), Index(1)); // LITTLE cores have half of grain
#endif
    this->half_range = range_size >> 1;
    this->min_steal  = MAX(Index(std::sqrt(range_size)), Index(1)); // minimal size to steal
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
#ifdef EXTRACT_THRESHOLD
    const Index chunk    = ((this->last - this->_working_last) > (this->seq_chunk << 2)) ? this->seq_chunk : 1;
    this->_working_first = MIN((this->copy_first + chunk), static_cast<Index>(this->last));
#else
    this->_working_first = MIN((this->copy_first + this->seq_chunk), static_cast<Index>(this->last));
#endif
    if (this->_working_first > this->_working_last) {
      this->first = this->_working_first;
      if (this->_working_first < this->last) {
        this->_working_last  = this->_working_first;
        const Index tmp      = this->copy_first;
        this->copy_first     = this->_working_first;
        this->_working_first = tmp;
        return true;
      }
      this->first = this->copy_first; // conflict
    }
    /* conflict detected: rollback and lock */
    this->lock.lock();
    this->_working_first = this->copy_first;
    if (this->_working_first < last) this->first = this->_working_last = this->copy_first = this->last;
    this->lock.unlock();
    return (this->_working_first < this->first);
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
    size_t remaining = this->_nthr - 1;
    size_t i;

    this->first = std::numeric_limits<Index>::max();
    this->last  = std::numeric_limits<Index>::max();

    std::array<bool, ADPT_MAX_THREADS> _visited;
    _visited.fill(false);
    _visited[this->_id] = true;

    // While there are sub-ranges that are not inspected yet
    while (remaining) {
      for (i = rand() % this->_nthr; _visited[i]; i = (i + 1) % this->_nthr)
        ; // Advances to an unvisited victim/sub-range
      Worker &victim = *(this->_workers_array->at(i));

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
              this->last  = vic_last;
              this->first = new_last;
              this->recalc_internal();
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

} // namespace __internal__
} // namespace adapt

#endif