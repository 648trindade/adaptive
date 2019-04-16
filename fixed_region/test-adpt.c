#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include "adaptive.h"

void func(void* data, size_t i){
    int* vec = (int*) data;
    int accum = 0;
    // Soma de 1 a i
    for (int j=1; j<=i+1; j++)
        accum += j;
    // teste inútil pro compilador não otimizar o accum
    if (accum == ((i+1)*(i+2)/2))
        // adiciona um dígito decimal referente ao número da thread
        vec[i] = vec[i] * 10 + omp_get_thread_num() + 1;
}

int main(int argc, char* argv[]){
    int s = 1000;
    int* data = malloc(sizeof(int) * s);
    int run = 1;

    while(run) {
        if (run++ % 100 == 0)
            printf("Run %d\n", run-1);

        for(size_t j = 0; j < s; j++)
            data[j] = 0;

        adpt_parallel_for(func, data, 0, s);
        
        for(size_t j = 0; j < s; j++)
            if (data[j] > 9 || data[j] == 0){
                printf("### iter %lu valor %d\n", j, data[j]);
                run = 0;
            }
    }

    free(data);
    return 0;
}
