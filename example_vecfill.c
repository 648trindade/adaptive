#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _OPENMP
#include "adaptive.h"
#endif

int *data;

#ifndef _OPENMP
void kernel(const size_t i_begin, const size_t i_end) {
  for (size_t i = i_begin; i < i_end; i++) data[i] = data[i] * 10 + omp_get_thread_num() + 1;
}
#endif

int main(int argc, char *argv[]) {
  size_t s = 1000;
  data     = malloc(s * sizeof(int));
  int run  = 1;

  while (run) {
    if (run++ % 10000 == 0) printf("Run %d\n", run - 1);
    // run++;

    for (size_t j = 0; j < s; j++) data[j] = 0;

#ifndef _OPENMP
    c_parallel_for(kernel, 0ul, s);
#else
#pragma omp parallel for schedule(dynamic)
    for (size_t i = 0; i < s; i++) { data[i] = data[i] * 10 + omp_get_thread_num() + 1; }
#endif

    for (size_t j = 0; j < s; j++)
      if (data[j] > 9 || data[j] == 0) {
        printf("### ERRO iter %lu valor %d\n", j, data[j]);
        run = 0;
      }
  }

  return 0;
}