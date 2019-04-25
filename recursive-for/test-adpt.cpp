#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include "recursive-for.hpp"

int main(int argc, char* argv[]){
    size_t s = 1000;
    std::vector<int> data(s, 0);
    int run = 1;

    while(run) {
        if (run++ % 10000 == 0)
           printf("Run %d\n", run-1);
        // run++;

        std::fill(data.begin(), data.end(), 0);

        parallel_for(0ul, s, [&data](size_t i){
            data[i] = data[i] * 10 + omp_get_thread_num() + 1;
        });
        
        for(size_t j = 0; j < s; j++)
            if (data.at(j) > 9 || data.at(j) == 0){
                printf("### ERRO iter %lu valor %d\n", j, data.at(j));
                run = 0;
            }
    }

    return 0;
}
