#include "../adaptive/adaptive.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <numeric>
#include <omp.h>
#include <tbb/parallel_for.h>
#include <vector>

void avg_stddev(std::vector<double> &times, double &sum, double &avg, double &stddev) {
  sum             = std::accumulate(times.begin(), times.end(), 0.0);
  avg             = sum / times.size();
  double variance = 0.0;
  for (double t : times) variance += std::pow(t - avg, 2);
  stddev = std::sqrt(variance);
}

int main(int argc, char *argv[]) {
  size_t s = 1000;
  std::vector<int> data;
  int run           = 1;
  size_t log_update = 10000;
  size_t count      = 0;
  size_t limit      = 1000000000;
  double sum, avg, stddev;

  switch (argc) {
  case 4: log_update = static_cast<size_t>(atoi(argv[3]));
  case 3: s = static_cast<size_t>(atoi(argv[2]));
  case 2: limit = static_cast<size_t>(atoi(argv[1]));
  default: break;
  }
  data.resize(s);
  std::vector<double> times(limit);

  // Adaptive ==========================================================================================================

  for (count = 0; count < limit; count++) {
    if (count % log_update == 0) printf("Run Adaptive %zu\n", count);
    std::fill(data.begin(), data.end(), 0);

    double init = omp_get_wtime();

    adapt::parallel_for(size_t(0), s, [&](size_t i_begin, size_t i_end) {
      for (size_t i = i_begin; i < i_end; i++) data[i] += count * 10;
    });

    double end   = omp_get_wtime();
    times[count] = end - init;
  }
  avg_stddev(times, sum, avg, stddev);
  printf("ADAPT %zu iters - %lf s (avg %lf s) (stddev %lf s)\n", count, sum, avg, stddev);

  // TBB ===============================================================================================================

  for (count = 0; count < limit; count++) {
    if (count % log_update == 0) printf("Run TBB %zu\n", count);
    std::fill(data.begin(), data.end(), 0);

    double init = omp_get_wtime();

    tbb::parallel_for(tbb::blocked_range<int>(0, s), [&](tbb::blocked_range<int> r) {
      for (int i = r.begin(); i < r.end(); i++) data[i] += count * 10;
    });

    double end   = omp_get_wtime();
    times[count] = end - init;
  }
  avg_stddev(times, sum, avg, stddev);
  printf("TBB   %zu iters - %lf s (avg %lf s) (stddev %lf s)\n", count, sum, avg, stddev);

  // OpenMP ===========================================================================================================

  for (count = 0; count < limit; count++) {
    if (count % log_update == 0) printf("Run OpenMP %zu\n", count);
    std::fill(data.begin(), data.end(), 0);

    double init = omp_get_wtime();

#pragma omp parallel for schedule(static)
    for (int i = 0; i < s; i++) data[i] += count * 10;

    double end   = omp_get_wtime();
    times[count] = end - init;
  }
  avg_stddev(times, sum, avg, stddev);
  printf("OMP   %zu iters - %lf s (avg %lf s) (stddev %lf s)\n", count, sum, avg, stddev);

  return 0;
}
