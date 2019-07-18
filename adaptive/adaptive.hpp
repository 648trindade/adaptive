#include <vector> 
#include <omp.h>    //OpenMP
#include <string.h> //memset
#include <atomic>
#include <cmath>
#include <algorithm>
#include <memory>
#include <limits>

#define ADPT_ALPHA 1

#define WorkerPtr Worker<Function, Index>
#define WorkerVec std::shared_ptr<std::vector<WorkerPtr>>

namespace adapt {

  namespace //anonymous namespace
  {

#ifdef DDEBUG
    typedef struct{
      double time;
      char type;
      size_t wf, wl, f, l;
    }trace_t;
#endif

    template <class Function, class Index>
    class Worker{
    private:
      Index _nthr;
      Index _id; // thread ID
      Index _seq_chunk; // chunk/grain size
      Index _working_first;
      Index _working_last;
      WorkerVec _workers_array;

    public:
      std::atomic<Index> first; // first iteration of sub-range
      std::atomic<Index> last; // last (+1) iteration of sub-range
      Index min_steal; // square root of su-range size
      omp_lock_t lock; // worker lock
#ifdef DDEBUG
      trace_t trace[400];
size_t count;
      #endif

      Worker(){
        omp_init_lock(&lock);
      }

      ~Worker(){
        omp_destroy_lock(&lock);
      }

      void initialize(
        size_t thr_id, Index global_first, Index global_last, WorkerVec& workers_array
      ){
        _id = thr_id;
        _nthr = omp_get_max_threads();
        _workers_array = workers_array;
        
        const Index chunk  = (global_last - global_first) / _nthr; // size of initial sub-range (integer division)
        const Index remain = (global_last - global_first) % _nthr; // remains of integer division
        first = chunk * _id + global_first + std::min(_id, remain); // first iteration of sub-range
        last = first + chunk + static_cast<Index>(_id < remain); // last (+1) iteration of sub-range
        _working_first = _working_last = first;
        
        const Index m = last - first;  // original size of sub-range
        _seq_chunk = std::log2(m);  // chunk/grain size to serial extraction
        min_steal = std::sqrt(m);  // minimal size to steal
#ifdef DDEBUG
        count = 0;
#endif
      }

    private:

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
      bool extract_seq(){
        const Index old_first = first;
        _working_first = std::min(old_first + _seq_chunk, static_cast<Index>(last));
        first = _working_first;
        if ((_working_first < last) && (_working_first > _working_last)){
          _working_last  = _working_first;
          _working_first = old_first;
#ifdef DDEBUG
          trace[count++] = {omp_get_wtime(), 'x', _working_first, _working_last, first, last};
#endif
          return true;
        }
        /* conflict detected: rollback and lock */
        first = old_first;
        omp_set_lock(&(lock));
        _working_first = old_first;
        if (_working_first < last)
          first = _working_last = last;
        omp_unset_lock(&(lock));

#ifdef DDEBUG
        trace[count++] = {omp_get_wtime(), 'r', _working_first, _working_last, first, last};
#endif
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
      bool extract_par(){
        bool success = false;
        size_t remaining = _nthr - 1;
        size_t i;
        
        first = std::numeric_limits<Index>::max();

        std::vector<bool> _visited(_nthr, false);
        _visited[_id] = true;

        // While there are sub-ranges that are not inspected yet
        while(remaining && !success){
          for (i = rand() % _nthr; _visited[i]; i = (i+1) % _nthr); // Advances to an unchecked sub-range
          WorkerPtr& victim = _workers_array->at(i);

          const Index pre_chunk_size = (victim.last - victim.first) >> 1;
          const Index pre_steal_size = (pre_chunk_size >= victim.min_steal) \
                                      ? pre_chunk_size : static_cast<Index>(victim.last);
          // Tests if the sub-range is not locked and locks it
          if ((victim.last - pre_steal_size) > victim.first) {
            if (omp_test_lock(&(victim.lock))){
              const Index chunk_size = (victim.last - victim.first) >> 1;
              const Index _last = victim.last - chunk_size;
              last = _last;
              victim.last = _last;
              if (last <= victim.first){
                /* rollback and abort */
                victim.last = _last + chunk_size;
                --remaining;
                _visited[i] = true;
#ifdef DDEBUG
                trace[count++] = {omp_get_wtime(), 't', _last, _last + chunk_size, victim.first, victim.last};
#endif
              }
              else{
                first = _last;
                last  = _last + chunk_size;
                min_steal  = std::sqrt(chunk_size);
                min_steal  = std::max(min_steal, static_cast<Index>(4));
                _seq_chunk = std::log2(chunk_size);
                _seq_chunk = std::max(_seq_chunk, static_cast<Index>(1));
                success = true;
#ifdef DDEBUG
                trace[count++] = {omp_get_wtime(), 's', _last, _last + chunk_size, victim.first, victim.last};
#endif
              }
              omp_unset_lock(&(victim.lock));
            }
          }
          else {
            --remaining;
            _visited[i] = true;
          }
        }
        return success;
      }

    public:

      void work(Function kernel){
        while(true){ // Iterates while there are work to be done
          while (extract_seq()) // Iterates while there are sequential work to be done
            for (Index i = _working_first; i < _working_last; i++)
              kernel(i);
          
          // Tries to steal work. If there are no work to steal, exits
          if (!extract_par())
            return;
        }
      }
    };

  }

  /*
  * Function: adapt::parallel_for
  * ---------------------------
  * 
  *    first : beggining of loop
  *     last : end of loop
  *   kernel : loop body
  */
  template <class Function, class Index>
  void parallel_for(Index first, Index last, Function kernel){
    WorkerVec workers = std::make_shared<std::vector<WorkerPtr>>(omp_get_max_threads()); // Data for workers

    #pragma omp parallel shared(kernel, workers, first, last)
    {
      size_t  thr_id = omp_get_thread_num();  // thread ID
      WorkerPtr& m_worker = workers->at(thr_id); // retrieve this thread's worker
      m_worker.initialize(thr_id, first, last, workers); // initializes the worker
      
      #pragma omp barrier
      m_worker.work(kernel);
    }

#ifdef DDEBUG
    for (int i=0; i< omp_get_max_threads(); i++){
      WorkerPtr& worker = workers->at(i);
      for (int j=0; j<worker.count; j++)
        fprintf(stderr, "%.12lf %d %c %lu %lu %lu %lu\n", worker.trace[j].time, i, worker.trace[j].type, worker.trace[j].wf, worker.trace[j].wl, worker.trace[j].f, worker.trace[j].l);
    }
#endif
    workers->clear();
  }

};

#undef WorkerPtr
#undef WorkerVec