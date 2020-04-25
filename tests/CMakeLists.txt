add_executable(
    test_suite 
    "test_suite.cpp"
    "test_internal_functions.cpp"
    "test_parallel_primitives.cpp"
)
target_link_libraries(test_suite adaptive)

# include(CTest)
find_package(Catch REQUIRED)
catch_discover_tests(test_suite)