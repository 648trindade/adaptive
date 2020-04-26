#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_CONSOLE_WIDTH 120
#include "../adaptive/adaptive.hpp"
#include "Catch2/catch.hpp"

static auto for_compute       = [](const int b, const int e) {};
static auto reduction_compute = [](const int b, const int e, int i) { return i; };
typedef std::__1::plus<int> plus_int;
typedef decltype(for_compute) for_body;
typedef decltype(reduction_compute) red_body;

static adapt::__internal__::ForWorker<int, for_body> *
create_dumb_forworker(const int id, const int first, const int last) {
  using namespace adapt::__internal__;
  using forworker_t = ForWorker<int, for_body>;
  return new forworker_t(id, first, last, nullptr, for_compute);
}

static adapt::__internal__::ReductionWorker<int, red_body, int, plus_int> *
create_dumb_reductionworker(const int id, const int first, const int last) {
  using namespace adapt::__internal__;
  plus_int reductor = std::plus<int>();
  using redworker_t = ReductionWorker<int, red_body, int, plus_int>;
  return new redworker_t(id, first, last, 0, nullptr, reduction_compute, reductor);
}

TEST_CASE("Get Number of Threads") {
  /* Without explicit configuration by environment variable ADPT_NUM_THREADS, it should return the same value as
   * std::thread::hardware_concurrency */
  REQUIRE(adapt::get_num_threads() == std::thread::hardware_concurrency());
}

TEST_CASE("Thread Handler Object") {
  using namespace adapt::__internal__;
  ThreadHandler &th  = thread_handler;
  size_t num_threads = adapt::get_num_threads();

  SECTION("dumb test to synchronize thread creation") {
    // dumb parallel for to ensure scheduler thread creation before internal tests
    adapt::parallel_for(0, 1, [](const int b, const int e) {});
  }

  SECTION("is the master thread the same main thread") { CHECK(th.master == pthread_self()); }

  SECTION("does the counter have the correct number of threads") { CHECK(th.counter == num_threads); }

  SECTION("does the cpusets the right cpu configuration") {
    bool passed = true;
    for (int i = 0; (i < num_threads) && passed; i++) {
      const cpu_set_t &cpuset = th.cpusets[i];
      for (int j = 0; (j < num_threads) && passed; j++)
        if (CPU_ISSET(j, &cpuset) && (i != j)) passed = false;
    }
    CHECK(passed);
  }

  SECTION("do each thread has a worker") {
    bool passed = true;
    for (int i = 0; (i < ADPT_MAX_THREADS) && passed; i++)
      if (((i < num_threads) && (th.absWorkers[i] == nullptr)) || ((i >= num_threads) && (th.absWorkers[i] != nullptr)))
        passed = false;
    CHECK(passed);
  }

  SECTION("destructing object") {
    ThreadHandler *nth = new ThreadHandler();
    delete nth;
  }
}

TEST_CASE("For Workers") {
  using namespace adapt::__internal__;
  ThreadHandler &th  = thread_handler;
  size_t num_threads = adapt::get_num_threads();
  const int first = 0, last = num_threads;

  std::vector<ForWorker<int, for_body> *> workers(num_threads, nullptr);
  for (size_t i = 0; i < num_threads; i++) workers[i] = create_dumb_forworker(i, first, last);

  SECTION("checking internals after initialization") {
    const int chunk  = (last - first) / num_threads; // size of initial sub-range (integer division)
    const int remain = (last - first) % num_threads; // remains of integer division
    for (size_t i = 0; i < num_threads; i++) {
      auto *worker = workers[i];
      CHECK(worker->first == chunk * i + first + MIN(i, remain));                  // first iteration of sub-range
      CHECK(worker->last == worker->first + chunk + static_cast<int>(i < remain)); // last (+1) iteration of sub-range
      CHECK(worker->seq_chunk == grain_number); // chunk/grain size to serial extraction
      CHECK(worker->min_steal == MAX(int(std::sqrt(worker->last - worker->first)), 1)); // minimal size to steal
      delete worker;
    }
  }
}

TEST_CASE("Reduction Workers") {
  using namespace adapt::__internal__;
  ThreadHandler &th  = thread_handler;
  size_t num_threads = adapt::get_num_threads();
  const int first = 0, last = num_threads;

  std::vector<ReductionWorker<int, red_body, int, std::__1::plus<int>> *> workers(num_threads, nullptr);
  for (size_t i = 0; i < num_threads; i++) workers[i] = create_dumb_reductionworker(i, first, last);

  SECTION("checking internals after initialization") {
    int first = 0, last = num_threads;
    const int chunk  = (last - first) / num_threads; // size of initial sub-range (integer division)
    const int remain = (last - first) % num_threads; // remains of integer division
    for (size_t i = 0; i < num_threads; i++) {
      auto *worker = workers[i];
      CHECK(worker->first == chunk * i + first + MIN(i, remain));                  // first iteration of sub-range
      CHECK(worker->last == worker->first + chunk + static_cast<int>(i < remain)); // last (+1) iteration of sub-range
      CHECK(worker->seq_chunk == grain_number); // chunk/grain size to serial extraction
      CHECK(worker->min_steal == MAX(int(std::sqrt(worker->last - worker->first)), 1)); // minimal size to steal
      CHECK(worker->reduction_value == 0);
      delete worker;
    }
  }
}