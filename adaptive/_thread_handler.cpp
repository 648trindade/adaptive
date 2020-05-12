#include "_thread_handler.hpp"

#include <pthread.h>
#include <thread>

namespace adapt {
namespace __internal__ {

static bool _is_workers_running = true;

static size_t get_from_env(std::string var, size_t default_value) {
  size_t value        = default_value;
  const char *env_var = getenv(var.c_str());
  if (env_var && atoi(env_var)) { value = atoi(env_var); }
  return value;
}

static void pin_thread(cpu_set_t *cpuset, const int thread_id) {
  const int core = thread_id % std::max(std::thread::hardware_concurrency(), 1u);
  // printf("Pinning Thread %d on core %d\n", thread_id, core);
  CPU_SET(core, cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), cpuset);
}

ThreadHandler::ThreadHandler() : stop(true), counter(1) {
  this->master      = pthread_self();
  this->num_threads = get_from_env("ADAPT_NUM_THREADS", std::max(std::thread::hardware_concurrency(), 1u));
  this->alpha       = get_from_env("ADAPT_ALPHA", 1);
#ifdef GRAIN_FRACTION
  this->grain = get_from_env("ADAPT_GRAIN", 256);
#else
  this->grain = get_from_env("ADAPT_GRAIN", 1);
#endif

  this->start_threads();
}

ThreadHandler::~ThreadHandler() { this->stop_threads(); }

inline bool ThreadHandler::in_master() { return this->master == pthread_self(); }

void ThreadHandler::start_threads() {
  if (this->stop) {
    this->stop = false;
    this->barrier.set_participants(this->num_threads);
    for (size_t i = 1; i < this->num_threads; i++) {
      pthread_create(&this->threads[i], nullptr, ThreadHandler::spawn_worker, this);
    }
    ThreadHandler::spawn_worker(this);
  }
}

void ThreadHandler::stop_threads() {
  if (!this->stop) {
    this->stop    = true; // tells other threads to stop
    this->counter = 1;    // now only running on 1 thread
    this->barrier.wait(); // free barrier
    for (size_t i = 1; i < this->num_threads; i++) {
      pthread_join(this->threads[i], nullptr);
      this->threads[i] = 0;
    }
  }
}

void ThreadHandler::work(int my_id) {
  do {
    this->barrier.wait();  // wait for worker creation
    if (this->stop) break; // program exited

    WorkerInterface &m_worker = *(this->workers_array[my_id]);
    m_worker.work();

    if (!this->in_master()) this->barrier.wait(); // wait for posterior worker deletion
  } while (!this->in_master());                   // do not loop if master thread
}

// pthread callback
void *ThreadHandler::spawn_worker(void *ptr) {
  ThreadHandler *self = static_cast<ThreadHandler *>(ptr);
  int my_id           = (self->in_master()) ? 0 : self->counter++;
  pin_thread(&self->cpusets[my_id], my_id);
  if (my_id) self->work(my_id);
  return nullptr;
}

ThreadHandler thread_handler;

size_t get_alpha() { return thread_handler.alpha; }

size_t get_grain() { return thread_handler.grain; }

} // namespace __internal__

size_t get_num_threads() { return __internal__::thread_handler.num_threads; }

void start_workers() { __internal__::thread_handler.start_threads(); }

void stop_workers() { __internal__::thread_handler.stop_threads(); }

} // namespace adapt