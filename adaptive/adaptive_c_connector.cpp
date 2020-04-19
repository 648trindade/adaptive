#include "adaptive.h"
#include "adaptive.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void c_parallel_for(void (*kernel)(size_t, size_t), size_t first, size_t last) {
  adapt::parallel_for(first, last, kernel);
}

#ifdef __cplusplus
} // extern "C"
#endif