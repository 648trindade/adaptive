add_executable(test_internal_functions  "test_internal_functions.cpp")
target_link_libraries(test_internal_functions adaptive)

add_executable(test_barriers "test_barriers.cpp")
target_link_libraries(test_barriers adaptive)

add_executable(test_locks "test_locks.cpp")
target_link_libraries(test_locks adaptive)

# include(CTest)
find_package(Catch REQUIRED)
catch_discover_tests(test_internal_functions)
catch_discover_tests(test_barriers)
catch_discover_tests(test_locks)