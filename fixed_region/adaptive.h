#ifndef ADAPTIVE_H_
#define ADAPTIVE_H_

#include <stddef.h>

void adpt_parallel_for(
    void (*kernel)(void*, size_t), void* data, size_t first, size_t last
);

#endif //ADAPTIVE_H_