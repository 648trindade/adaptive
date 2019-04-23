#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include "adaptive.hpp"

void func(std::vector<int>& vec, size_t i){
    // int accum = 0;
    // // Soma de 1 a i
    // for (int j=1; j<=i+1; j++)
    //     accum += j;
    // // teste inútil pro compilador não otimizar o accum
    // if (accum == ((i+1)*(i+2)/2))
        // adiciona um dígito decimal referente ao número da thread
        //printf("%d %lu\n", omp_get_thread_num(), i);
        vec[i] = vec[i] * 10 + omp_get_thread_num() + 1;
}

int main(int argc, char* argv[]){
    size_t s = 1000;
    std::vector<int> data(s, 0);
    int run = 1;

    while(run && run < 100000) {
        //if (run++ % 1000 == 0)
        //    printf("Run %d\n", run-1);
        run++;

        std::fill(data.begin(), data.end(), 0);

        adpt_parallel_for(func, data, 0, s);
        
        for(size_t j = 0; j < s; j++)
            if (data.at(j) > 9 || data.at(j) == 0){
                printf("### ERRO iter %lu valor %d\n", j, data.at(j));
                run = 0;
            }
    }

    return 0;
}
