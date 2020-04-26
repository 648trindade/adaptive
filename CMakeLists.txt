PROJECT(Adaptive)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
set(CMAKE_MODULE_PATH "${Adaptive_SOURCE_DIR}/cmake")
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

SET(SOURCES 
    "adaptive/adaptive.hpp"
    "adaptive/_defines.hpp"
    "adaptive/_parallel_for_worker.hpp"
    "adaptive/_reduction_worker.hpp"
    "adaptive/_thread_handler.cpp"
    "adaptive/_thread_handler.hpp"
    "adaptive/_worker.hpp"
    "adaptive/atomic_barrier.cpp"
    "adaptive/atomic_barrier.hpp"
    "adaptive/atomic_mutex.cpp"
    "adaptive/atomic_mutex.hpp"
    )
SET(CONNECTOR_SOURCES "adaptive/adaptive_c_connector.cpp" "adaptive/adaptive.h")

ADD_LIBRARY(adaptive SHARED ${SOURCES} ${CONNECTOR_SOURCES})
target_link_libraries(adaptive Threads::Threads)
include_directories("include")

add_subdirectory(tests)
add_subdirectory(examples)

install(
    TARGETS adaptive
    LIBRARY
        DESTINATION "lib"
        COMPONENT Libraries
)
install(
    FILES 
        "adaptive/adaptive.hpp"
        "adaptive/adaptive.h"
        "adaptive/_defines.hpp"
        "adaptive/_parallel_for_worker.hpp"
        "adaptive/_reduction_worker.hpp"
        "adaptive/_thread_handler.hpp"
        "adaptive/_worker.hpp"
        "adaptive/atomic_barrier.hpp"
        "adaptive/atomic_mutex.hpp"
    DESTINATION "include/adaptive"
)