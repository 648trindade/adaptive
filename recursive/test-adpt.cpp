#include <omp.h>
#include <iostream>
#include <cstdlib>
#include <vector>
#include "for_each.hpp"

using namespace std;


int main(int argc, char* argv[]){
    int s = 1000;
    vector<int> data(1000);
    int run = 1;

    while(run && run < 100000) {
        // if (run++ % 1 == 0)
        //     cout << "Run " << (run-1) << endl;
        run++;

        for(size_t j = 0; j < s; j++)
            data[j] = 0;

        adapt::for_each(begin(data), end(data), [](int& i){
            i = i * 10 + omp_get_thread_num() + 1;
        });
        
        for(size_t j = 0; j < s; j++)
            if (data[j] > 9 || data[j] == 0){
                cout << "### ERRO iter " << j << " valor " << data[j] << endl;
                run = 0;
            }
    }
    return 0;
}
