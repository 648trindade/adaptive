find_package(OpenMP REQUIRED)
find_package(TBB REQUIRED)

add_executable(reduction "example_reduction.cpp")
add_executable(sincos "example_sincos.cpp")
add_executable(vecfill "example_vecfill.cpp")
add_executable(vecfill_c "example_vecfill.c")

target_link_libraries(reduction adaptive OpenMP::OpenMP_CXX tbb)
target_link_libraries(sincos    adaptive OpenMP::OpenMP_CXX tbb)
target_link_libraries(vecfill   adaptive OpenMP::OpenMP_CXX tbb)
target_link_libraries(vecfill_c adaptive OpenMP::OpenMP_CXX tbb)