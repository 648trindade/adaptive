#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include "recursive-for.h"

int* data;

void kernel(size_t i){
    data[i] = data[i] * 10 + omp_get_thread_num() + 1;
}

int main(int argc, char* argv[]){
    size_t s = 1000;
    data = malloc(s * sizeof(int));
    int run = 1;

    while(run) {
        if (run++ % 10000 == 0)
           printf("Run %d\n", run-1);
        // run++;

        for(size_t j = 0; j < s; j++)
            data[j] = 0;


        c_parallel_for(kernel, 0ul, s);
        
        for(size_t j = 0; j < s; j++)
            if (data[j] > 9 || data[j] == 0){
                printf("### ERRO iter %lu valor %d\n", j, data[j]);
                run = 0;
            }
    }

    return 0;
}