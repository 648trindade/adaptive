# Adaptive

Adaptive is a parallel loop scheduler deisgned with an adaptive algorithm. Its original purpose is to better balancing parallel irregular workloads and/or better balancing regular (and irregular) workloads on Asymmetric Multicore Processors (AMP).

The hybrid adaptive scheduling algorithm uses _work stealing_ in order to balance loads on threads, and uses a THE protocol approach to minimize parallel overhead on concurrent scheduling operations. A deep explanation of how scheduler actually works can be found on the following master thesis (written in portuguese. English paper coming soon):

> Trindade, Rafael G. and Lima, João V. F.. **Escalonador Adaptativo de Laços Paralelos para Processadores Multinúcleo Assimétricos**. Master Thesis. Universidade Federal de Santa Maria. 2020. Santa Maria, RS, Brazil. Available at: http://www.inf.ufsm.br/~rtrindade/docs/dissertacao-rtrindade.pdf.
>
> English Title: **An Adaptive Scheduler of Parallel Loops for Asymmetric Multicore Processors**

## API

The Adaptive's API is based on Thread Building Blocks API. Currently we support two kinds of parallel loop:

* Common parallel loops:
```c++
void adapt::parallel_loop(
    T start, T end, 
    void body(T, T)
);
```
* Reduction parallel loops
```c++
V adapt::parallel_reduce(
    T start, T end, 
    V initial, 
    V body(T, T, V), 
    V reductor(V, V)
);
```

The API accepts functions and lambda functions as parameters for `body` and `reductor` arguments.

## Examples

Parallelizing a vector filling algorithm
```c++
std::vector<int> vec(256, 0);

adapt::parallel_for(
    0, 256,
    [&vec](const int start, const int end) {
        for (int i = start; i < end; i++)
            vec[i] = some_function();
    }
);
```

Parallelizing a vector sum algorithm
```c++
std::vector<double> vec(256, 0.0);

// ... fill vec someway

double result = adapt::parallel_reduce(
    0, 256, 0.0,
    // body
    [&vec](const int start, const int end, double initial) {
        double _result = initial;
        for (int i = start; i < end; i++)
            _result += vec[i];
        return _result;
    },
    // reductor
    std::plus<double>();
);
```

Parallelizing a find minimal value and index algorithm
```c++
std::vector<double> vec(256, 0.0);

// ... fill vec someway

std::pair<int, double> _initial = std::make_pair(0, vec[0]);

std::pair<int, double> result = adapt::parallel_reduce(
    1, 256, _initial,
    // body
    [&vec](const int start, const int end, std::pair<int, double> initial) {
        std::pair<int, double> _result = initial;
        for (int i = start; i < end; i++) {
            if (vec[i] < _result.second) {
                _result.first = i;
                _result.second = vec[i];
            }
        }
        return _result;
    },
    // reductor
    [](const std::pair<int, double> left, const std::pair<int, double> right) {
        std::pair<int, double> _result = left;
        if (right.second < _result.second) {
            _result.first = right.first;
            _result.second = right.second;
        }
        return _result;
    }
);
```