#include "adaptive.hpp"

#include <thread>

namespace adapt {
namespace __internal__ {
size_t num_threads  = 1;
size_t alpha        = 1;
size_t grain_number = 1;

static size_t get_concurrency() {
  size_t num_threads      = 1;
  const char *nthr_envvar = getenv("ADPT_NUM_THREADS");
  if (nthr_envvar && atoi(nthr_envvar)) num_threads = atoi(nthr_envvar);
  else
    num_threads = std::max(std::thread::hardware_concurrency(), 1u); // may return 0 when not able to detect
  return num_threads;
}

static size_t get_alpha() {
  size_t alpha             = 1;
  const char *alpha_envvar = getenv("ADPT_ALPHA");
  if (alpha_envvar && atoi(alpha_envvar)) alpha = atoi(alpha_envvar);
  return alpha;
}

static size_t get_grain() {
#ifdef GRAIN_FRACTION
  size_t grain = 256;
#else
  size_t grain = 1;
#endif
  const char *grain_envvar = getenv("ADPT_GRAIN");
  if (grain_envvar && atoi(grain_envvar)) grain = atoi(grain_envvar);
  return grain;
}

// AtomicBarrier =======================================================================================================

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

// AtomicMutex ========================================================================================================

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

ThreadHandler::ThreadHandler() : stop(false), counter(1) {
  master       = pthread_self();
  num_threads  = get_concurrency();
  alpha        = get_alpha();
  grain_number = get_grain();

  barrier.set_participants(num_threads);
  for (size_t i = 1; i < num_threads; i++) pthread_create(&threads[i], nullptr, ThreadHandler::spawn_worker, this);
  ThreadHandler::spawn_worker(this);
}

ThreadHandler::~ThreadHandler() {
  stop = true;
  work(0);
  for (size_t i = 1; i < num_threads; i++) pthread_join(threads[i], nullptr);
}

void ThreadHandler::work(int my_id) {
  do {
    barrier.wait();  // wait for worker creation
    if (stop) break; // program exited

    AbstractWorker &m_worker = *(absWorkers[my_id]);
    m_worker.work();

    if (my_id) barrier.wait(); // wait for posterior worker deletion
  } while (my_id);             // do not loop if master thread
}

static void pin_thread(cpu_set_t *cpuset, const int thread_id) {
  const int core = thread_id % std::max(std::thread::hardware_concurrency(), 1u);
  // printf("Pinning Thread %d on core %d\n", thread_id, core);
  CPU_SET(core, cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), cpuset);
}

// pthread callback
void *ThreadHandler::spawn_worker(void *ptr) {
  ThreadHandler *self = static_cast<ThreadHandler *>(ptr);
  int my_id           = (self->master == pthread_self()) ? 0 : self->counter++;
  pin_thread(&self->cpusets[my_id], my_id);
  if (my_id) self->work(my_id);
  return nullptr;
}

ThreadHandler thread_handler;
} // namespace __internal__

size_t get_num_threads() { return __internal__::num_threads; }

} // namespace adapt