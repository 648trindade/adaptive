#include "../adaptive/adaptive.h"

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int *data;

void kernel(const size_t i_begin, const size_t i_end) {
  for (size_t i = i_begin; i < i_end; i++) data[i] = data[i] * 10 + omp_get_thread_num() + 1;
}

void adaptive_version(size_t s) { c_parallel_for(kernel, 0ul, s); }

void omp_version(size_t s) {
#pragma omp parallel for schedule(dynamic)
  for (size_t i = 0; i < s; i++) { data[i] = data[i] * 10 + omp_get_thread_num() + 1; }
}

int main(int argc, char *argv[]) {
  size_t s = 1000;
  data     = malloc(s * sizeof(int));
  int run  = 1;
  double t0, t1;

  // Adaptive
  t0 = omp_get_wtime();
  for (run = 1; run < 1000; run++) {
    if (run % 10000 == 0) { printf("Run %d\n", run); }
    for (size_t j = 0; j < s; j++) { data[j] = 0; }
    adaptive_version(s);
  }
  t1 = omp_get_wtime();
  printf("Adaptive: %lf secs\n", t1 - t0);

  // OpenMP
  t0 = omp_get_wtime();
  for (run = 1; run < 1000; run++) {
    if (run % 10000 == 0) { printf("Run %d\n", run); }
    for (size_t j = 0; j < s; j++) { data[j] = 0; }
    omp_version(s);
  }
  t1 = omp_get_wtime();
  printf("OpenMP  : %lf secs\n", t1 - t0);

  return 0;
}