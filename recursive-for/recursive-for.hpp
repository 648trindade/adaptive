#pragma once

#include <omp.h>
#include <cmath>
#include <iostream>

template<class Function, class Index>
void recursive_for(Index low, Index high, Index grain, Function& body){
    Index mid, count = high - low;
    while (count > grain){
        mid = low + count / 2;
#pragma omp task firstprivate(low, mid) shared(grain, body)
        recursive_for(low, mid, grain, body);
        low = mid;
        count = high - low;
    }
    for (Index i=low; i<high; i++)
        body(i);
}

template<class Function, class Index>
void parallel_for(const Index low, const Index high, const Function& body){
    const Index grain = std::log2(high - low);
#pragma omp parallel
#pragma omp single
    recursive_for(low, high, grain, body);
}