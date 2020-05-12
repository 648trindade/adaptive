#pragma once

#ifndef _DEFINES_HPP_
#define _DEFINES_HPP_

#include <unistd.h>

// 2 big (1 ~ 2), 4 LITTLE (0, 3 ~ 5)
#ifdef NVIDIA_JETSON_TX2
#define IS_LITTLE(id) ((id == 0) || (id > 2))

// 4 big (4 ~ 7) 4 LITTLE (0 ~ 3)
#elif ODROID_XU3_LITE
#define IS_LITTLE(id) (id < 4)

// no big.LITTLE
#else
#define IS_LITTLE(id) (id < 2)
#endif

#define MIN(x, y) ((x < y) ? x : y)
#define MAX(x, y) ((x > y) ? x : y)

#define ADPT_MAX_THREADS 256

namespace adapt {

size_t get_num_threads();
void start_workers();
void stop_workers();

namespace __internal__ // anonymous namespace
{

size_t get_grain();
size_t get_alpha();

} // namespace __internal__
} // namespace adapt

#endif