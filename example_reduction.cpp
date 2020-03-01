#include "adaptive.hpp"
#include <cstdio>
#include <functional>

int main(){
    double r = adapt::parallel_reduce(0, 10, 0.0, [](const int b, const int e, double t_r){
        double r = t_r;
        for (int i = b; i < e; i++)
            r += double(i);
        return r;
    }, std::plus<double>());
    printf("%lf\n", r);
}
