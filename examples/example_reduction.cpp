#include "../adaptive/adaptive.hpp"

#include <functional>
#include <iostream>

int main() {
  double r = adapt::parallel_reduce(
    0, 10, 0.0,
    [](const int begin, const int end, double _initial) {
      double _result = _initial;
      for (int i = begin; i < end; i++) _result += double(i);
      return _result;
    },
    std::plus<double>());
  std::cout << r << std::endl;
}
