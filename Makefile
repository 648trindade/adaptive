
CXX = clang++
CC = clang
CFLAGS = -Wall -O3
CXXFLAGS = $(CFLAGS) -std=c++11 -stdlib=libc++ #-DCONFIG_VERBOSE
LDFLAGS = -lomp -lpthread -lm

all: adpt omp tbb

adpt:
	$(CXX) adaptive/adaptive.cpp $(CXXFLAGS) -Iadaptive -c -o adaptive/adaptive.o
	$(CXX) example_vecfill.cpp adaptive/adaptive.o $(CXXFLAGS) -Iadaptive -o cpp_example_vecfill_adaptive $(LDFLAGS)
	$(CXX) example_sincos.cpp adaptive/adaptive.o $(CXXFLAGS) -Iadaptive -o cpp_example_sincos_adaptive $(LDFLAGS)
	$(CXX) example_reduction.cpp adaptive/adaptive.o $(CXXFLAGS) -Iadaptive -o cpp_example_reduction_adaptive $(LDFLAGS)
	$(CXX) adaptive/adaptive_c_connector.cpp $(CXXFLAGS) -fno-exceptions -Iadaptive -c -o adaptive/adaptive_c_connector.o
	$(CC) example_vecfill.c adaptive/adaptive_c_connector.o adaptive/adaptive.o $(CFLAGS) -lc++ -Iadaptive -o c_example_vecfill_adaptive $(LDFLAGS)

omp:
	$(CXX) example_vecfill.cpp $(CXXFLAGS) -fopenmp -o cpp_example_vecfill_omp $(LDFLAGS)
	$(CXX) example_sincos.cpp $(CXXFLAGS) -fopenmp -o cpp_example_sincos_omp $(LDFLAGS)
	$(CC) example_vecfill.c $(CFLAGS) -fopenmp -o c_example_vecfill_omp $(LDFLAGS)


clean:
	rm -rf *.o c_example_vecfill* cpp_example_vecfill* cpp_example_vecfill* adaptive/*.o
