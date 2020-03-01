#ifndef __ADAPTIVE_H__
#define __ADAPTIVE_H__

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void c_parallel_for(void (*kernel)(size_t, size_t), size_t first, size_t last);

#ifdef __cplusplus
} //extern "C"
#endif

#endif //__ADAPTIVE_H__