#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_CONSOLE_WIDTH 120
#include "../adaptive/atomic_mutex.hpp"
#include "Catch2/catch.hpp"

#include <thread>

using namespace adapt;

TEST_CASE("Locking and Unlocking") {
  AtomicMutex mutex;
  mutex.lock();
  REQUIRE(!mutex.try_lock()); // Locked: Cannot lock again
  mutex.unlock();
  REQUIRE(mutex.try_lock()); // Unocked: Can be locked
  mutex.unlock();
}

TEST_CASE("Concurrent Locking") {
  AtomicMutex mutex;
  int count             = 0;
  const int iters       = 100000000;
  const int num_threads = 4;
  std::thread threads[num_threads];
  auto lock_loop = [&mutex, &count, iters]() {
    for (int i = 0; i < iters; i++) {
      mutex.lock();
      count++;
      mutex.unlock();
    }
  };

  for (int t = 0; t < num_threads; t++) { threads[t] = std::thread(lock_loop); }
  for (int t = 0; t < num_threads; t++) { threads[t].join(); }
  REQUIRE(count == (iters * num_threads));
  REQUIRE(mutex.try_lock());
  mutex.unlock();
}