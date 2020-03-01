#include <numeric>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <memory>
#include <limits>
#include <omp.h>

#if ADAPT
#include "adaptive.hpp"
#elif TBB
#include <tbb/parallel_for.h>
#endif

void avg_stddev(std::vector<double>& times, double& sum, double& avg, double& stddev){
    sum = std::accumulate(times.begin(), times.end(), 0.0);
    avg = sum / times.size();
    double variance = 0.0;
    for (double t : times)
        variance += std::pow(t - avg, 2);
    stddev = std::sqrt(variance);
}

int main(int argc, char* argv[]){
    size_t s = 1000;
    std::vector<int> data;
    int run = 1;
    size_t log_update = 10000;
    size_t count = 0;
    size_t limit = 1000000000;

    switch (argc){
        case 4:  log_update = static_cast<size_t>(atoi(argv[3]));
        case 3:  s = static_cast<size_t>(atoi(argv[2]));
        case 2:  limit = static_cast<size_t>(atoi(argv[1]));
        default: break;
    }
    data.resize(s);
    std::vector<double> times(limit);

    while(run && (count < limit)) {
        if (count++ % log_update == 0)
           printf("Run %zu\n", count-1);

        std::fill(data.begin(), data.end(), 0);

        double init = omp_get_wtime();

#if ADAPT
        adapt::parallel_for(size_t(0), s, [&](size_t i_begin, size_t i_end){
            for (size_t i = i_begin; i < i_end; i++)
                data[i] += count * 10;
        });
#elif TBB
        tbb::parallel_for(tbb::blocked_range<int>(0,s), [&](tbb::blocked_range<int> r){
            for(int i=r.begin(); i<r.end(); i++)
                data[i] += count * 10;
        });
#else
#ifdef _OPENMP
        #pragma omp parallel for schedule(static)
#endif
        for(int i=0; i<s; i++)
            data[i] += count * 10;
#endif

        double end = omp_get_wtime();
        times[count-1] = end - init;

        for(size_t j = 0; j < s; j++)
            if (data.at(j) != count * 10) {
                printf("### ERRO iter %zu valor %d\n", j, data.at(j));
                run = 0;
            }
    }
    double sum, avg, stddev;
    avg_stddev(times, sum, avg, stddev);
    printf("%zu iters - %lf s (avg %lf s) (stddev %lf s)\n", count, sum, avg, stddev);
    return 0;
}
