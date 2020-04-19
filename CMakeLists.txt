PROJECT(Adaptive)
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

SET(SOURCES "adaptive/adaptive.cpp")
SET(CONNECTOR_SOURCES "adaptive/adaptive_c_connector.cpp")

ADD_LIBRARY(adaptive SHARED ${SOURCES} ${CONNECTOR_SOURCES})
target_link_libraries(adaptive Threads::Threads)

add_subdirectory(tests)
add_subdirectory(examples)

install(
    TARGETS adaptive
    LIBRARY
        DESTINATION "lib"
        COMPONENT Libraries
)
install(
    FILES "adaptive/adaptive.hpp" "adaptive/adaptive.h"
        DESTINATION "include/adaptive"
)