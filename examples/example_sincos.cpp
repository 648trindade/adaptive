
#include "../adaptive/adaptive.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <omp.h>
#include <random>
#include <tbb/parallel_for.h> // TBB
#include <vector>

void adaptive_version(std::vector<double> &v, size_t n) {
  adapt::parallel_for(size_t(0), n, [&v](size_t begin, size_t end) {
    for (size_t i = begin; i < end; i++) {
      double res = 0.0;
      for (auto j = 0; j < 10000; j++) res += cos(v[i]) + sin(v[i]);
      v[i] = res;
    }
  });
}

void tbb_version(std::vector<double> &v, size_t n) {
  tbb::parallel_for(size_t(0), n, [&v](size_t i) {
    double res = 0.0;
    for (auto j = 0; j < 10000; j++) res += cos(v[i]) + sin(v[i]);
    v[i] = res;
  });
}

void omp_version(std::vector<double> &v, size_t n) {
#pragma omp parallel for schedule(dynamic, 64)
  for (size_t i = 0; i < n; i++) {
    double res = 0.0;
    for (auto j = 0; j < 10000; j++) res += cos(v[i]) + sin(v[i]);
    v[i] = res;
  }
}

int main(int argc, char **argv) {
  std::vector<double> v(100), _v(100);
  size_t n = 100;

  if (argc > 1) {
    n = static_cast<size_t>(atoi(argv[1]));
    v.resize(n);
    _v.resize(n);
  }
  std::cout << "Size " << v.size() << "\n\n";

  std::iota(begin(_v), end(_v), 1.0);
  std::shuffle(begin(_v), end(_v), std::mt19937 {std::random_device {}()});

  double t0, t1;

  // Adaptive
  std::copy(_v.begin(), _v.end(), v.begin());
  t0 = omp_get_wtime();
  adaptive_version(v, n);
  t1 = omp_get_wtime();
  std::cout << "Adaptive: " << (t1 - t0) << " secs." << std::endl;

  // Thread Building Blocks
  std::copy(_v.begin(), _v.end(), v.begin());
  t0 = omp_get_wtime();
  tbb_version(v, n);
  t1 = omp_get_wtime();
  std::cout << "TBB: " << (t1 - t0) << " secs." << std::endl;

  // OpenMP
  std::copy(_v.begin(), _v.end(), v.begin());
  t0 = omp_get_wtime();
  omp_version(v, n);
  t1 = omp_get_wtime();
  std::cout << "OpenMP: " << (t1 - t0) << " secs." << std::endl;

  return 0;
}
