#include <omp.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include "for_each.hpp"

using namespace std;

void func(int& i, vector<int> vec){
    int accum = 0;
    // Soma de 1 a 1000
    for (int j=1; j<=1000; j++)
        accum += j;
    // teste inútil pro compilador não otimizar o accum
    if (accum == ((1000)*(1001)/2))
        // adiciona um dígito decimal referente ao número da thread
        i = i * 10 + omp_get_thread_num() + 1;
}

int main(int argc, char* argv[]){
    int s = 1000;
    vector<int> data(1000);
    int run = 1;

    while(run == 1) {
        if (run++ % 1 == 0)
            cout << "Run " << (run-1) << endl;

        for(size_t j = 0; j < s; j++)
            data[j] = 0;

        adapt::for_each(begin(data), end(data), func, data);
        
        for(size_t j = 0; j < s; j++)
            if (data[j] > 9 || data[j] == 0){
                cout << "### iter " << j << " valor " << data[j] << endl;
                run = 0;
            }
    }
    return 0;
}
