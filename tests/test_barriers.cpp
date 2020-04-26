#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_CONSOLE_WIDTH 120
#include "../adaptive/adaptive.hpp"
#include "Catch2/catch.hpp"

#include <chrono>
#include <thread>

using namespace adapt::__internal__;

void barrier_loop(AtomicBarrier &barrier, size_t iters) {
  for (size_t i = 0; i < iters; i++) { barrier.wait(); }
}

void barrier_wait(AtomicBarrier &barrier) { barrier.wait(); };

TEST_CASE("is correctly setting participants") {
  AtomicBarrier barrier;
  std::thread wait_barrier_0, wait_barrier_1;

  // 1 thread
  barrier.set_participants(1);
  wait_barrier_0 = std::thread(barrier_wait, std::ref(barrier));
  wait_barrier_0.join();
  REQUIRE(barrier.is_free());

  // 2 threads
  barrier.set_participants(2);
  wait_barrier_0 = std::thread(barrier_wait, std::ref(barrier));
  std::this_thread::sleep_for(std::chrono::seconds(1));
  REQUIRE(!barrier.is_free());
  wait_barrier_1 = std::thread(barrier_wait, std::ref(barrier));
  wait_barrier_0.join();
  wait_barrier_1.join();
  REQUIRE(barrier.is_free());
}

TEST_CASE("do it runs with 1 thread per cpu") {
  AtomicBarrier barrier;
  const size_t num_threads = adapt::get_num_threads();
  const size_t iters       = 4000 / num_threads;
  std::thread threads[num_threads];
  barrier.set_participants(num_threads);
  for (size_t i = 0; i < num_threads; i++) { threads[i] = std::thread(barrier_loop, std::ref(barrier), iters); }
  for (size_t i = 0; i < num_threads; i++) { threads[i].join(); }
  REQUIRE(barrier.is_free());
}

TEST_CASE("does it works inside parallel for") {
  AtomicBarrier barrier;
  const size_t num_threads = adapt::get_num_threads();
  barrier.set_participants(num_threads);
  adapt::parallel_for(0ul, num_threads, [&barrier](const size_t b, const size_t e) {
    for (int i = 0; i < 100; i++) { barrier.wait(); }
  });
  REQUIRE(barrier.is_free());
}

TEST_CASE("do it runs with a lot (256) of threads") {
  AtomicBarrier barrier;
  const size_t num_threads = 256;
  const size_t iters       = 4000 / num_threads;
  std::thread threads[num_threads];
  barrier.set_participants(num_threads);
  for (size_t i = 0; i < num_threads; i++) { threads[i] = std::thread(barrier_loop, std::ref(barrier), iters); }
  for (size_t i = 0; i < num_threads; i++) { threads[i].join(); }
  REQUIRE(barrier.is_free());
}

TEST_CASE("is internal resetting counter correctly") {
  AtomicBarrier barrier;
  const size_t num_threads = 2;
  const size_t iters       = 1;
  std::thread threads[num_threads];
  barrier.set_participants(num_threads);
  for (size_t i = 0; i < num_threads; i++) { threads[i] = std::thread(barrier_loop, std::ref(barrier), iters); }
  for (size_t i = 0; i < num_threads; i++) { threads[i].join(); }
  REQUIRE(barrier.is_free());
  barrier.reset();
  for (size_t i = 0; i < num_threads; i++) { threads[i] = std::thread(barrier_loop, std::ref(barrier), iters); }
  for (size_t i = 0; i < num_threads; i++) { threads[i].join(); }
  REQUIRE(barrier.is_free());
}