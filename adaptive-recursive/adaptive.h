#ifndef __PARALLEL_FOR_H__
#define __PARALLEL_FOR_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void c_parallel_for(void (*kernel)(size_t), size_t first, size_t last);

#ifdef __cplusplus
} //extern "C"
#endif

#endif //__PARALLEL_FOR_H__