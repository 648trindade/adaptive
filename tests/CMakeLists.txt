add_executable(test_internal_functions test_internal_functions.cpp)
target_link_libraries(test_internal_functions adaptive)

# include(CTest)
include(Catch2/Catch.cmake)
catch_discover_tests(test_internal_functions)