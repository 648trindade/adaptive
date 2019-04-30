#include "recursive-for.h"
#include "recursive-for.hpp"

#ifdef __cplusplus
extern "C" {
#endif

void c_parallel_for(void (*kernel)(size_t), size_t first, size_t last){
    parallel_for(first, last, kernel);
}

#ifdef __cplusplus
} //extern "C"
#endif