#pragma once

#include <omp.h>
#include <cmath>
#include <algorithm>

namespace adapt {

  namespace //anonymous namespace
  {
    template<class Function, class Index>
    void __parallel_for(Index low, Index high, Index grain, Function body){
      Index mid, count = high - low;
      while (count > grain){
        mid = low + count / 2;
        #pragma omp task firstprivate(high, mid, grain, body)
        __parallel_for(mid, high, grain, body);
        high = mid;
        count = high - low;
      }
      for (Index i=low; i<high; i++)
        body(i);
    }
  }

  template<class Function, class Index>
  void parallel_for(const Index low, const Index high, Function body){
    Index grain = std::max(static_cast<Index>(std::log2(high - low)), static_cast<Index>(1));
    #pragma omp parallel default(shared)
    #pragma omp single
    __parallel_for(low, high, grain, body);
  }
};