
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <omp.h>
#include <random>
#include <vector>

#if ADAPT
#include "adaptive.hpp"
#elif TBB
#include <tbb/parallel_for.h>
#endif

int main(int argc, char **argv) {
  std::vector<double> v(100);
  size_t n = 100;

  if (argc > 1) {
    n = static_cast<size_t>(atoi(argv[1]));
    v.resize(n);
  }
  // std::vector<int> vcopy(20000000);
  std::iota(begin(v), end(v), 1.0);

  std::shuffle(begin(v), end(v), std::mt19937{std::random_device{}()});
  // std::copy(begin(v), end(v), begin(vcopy));
  auto t0 = omp_get_wtime();

#if ADAPT
  adapt::parallel_for(size_t(0), n, [&v](size_t b, size_t e) {
    for (size_t i = b; i < e; i++) {
      double res = 0.0;
      for (auto j = 0; j < 10000; j++)
        res += cos(v[i]) + sin(v[i]);
      v[i] = res;
    }
  });
#elif TBB
  tbb::parallel_for(size_t(0), n, [&v](size_t i) {
    double res = 0.0;
    for (auto j = 0; j < 10000; j++)
      res += cos(v[i]) + sin(v[i]);
    v[i] = res;
  });
#else
#if _OPENMP
#pragma omp parallel for schedule(dynamic, 64)
#endif
  for (auto i = 0; i < n; i++) {
    double res = 0.0;
    for (auto j = 0; j < 10000; j++)
      res += cos(v[i]) + sin(v[i]);
    v[i] = res;
  }
#endif
  auto t1 = omp_get_wtime();
  auto time = t1 - t0;

#if 0
    std::cout << "Contents: ";
    for (auto i : vcopy)
        std::cout << i << ' ';
    std::cout << '\n';

    std::cout << "Contents (new): ";
    for (auto i : v)
        std::cout << i << ' ';
    std::cout << '\n';
#endif

#if 0
    if (std::equal(begin(v), end(v), begin(vcopy),
                   [](int a, int b) { return a == (b + 1); }))
        std::cout << "OK\n";
    else
        std::cout << "ERROR\n";
#endif

  std::cout << "Size " << v.size() << " Time(s): " << time << "\n";

  return 0;
}
