
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>
#include <cmath>
#include <omp.h>

#include "for_each.hpp"

int main(int argc, char **argv) {
    std::vector<double> v(100);
    int n;

    if(argc > 1){
      n = atoi(argv[1]);
      v.resize(n);
    }
    //std::vector<int> vcopy(10000000);
    std::iota(begin(v), end(v), 1.0);

    std::shuffle(begin(v), end(v), std::mt19937{std::random_device{}()});
    //std::copy(begin(v), end(v), begin(vcopy));

    auto t0 = omp_get_wtime();
    adapt::for_each(begin(v), end(v), [](double &x) { 
      double res = 0.0;
      for (auto i= 0; i < 10000; i++)
        res += cos(x) + sin(x);
      x = res;
    });
    auto t1 = omp_get_wtime();
    auto time = t1-t0;

#if 0
    std::cout << "Contents: ";
    for (auto i : vcopy)
        std::cout << i << ' ';
    std::cout << '\n';

    std::cout << "Contents (new): ";
    for (auto i : v)
        std::cout << i << ' ';
    std::cout << '\n';
#endif

#if 0
    if (std::equal(begin(v), end(v), begin(vcopy),
                   [](int a, int b) { return a == (b + 1); }))
        std::cout << "OK\n";
    else
        std::cout << "ERROR\n";
#endif

    std::cout << "Size " << v.size() << " Time(s): " << time << "\n";

    return 0;
}
