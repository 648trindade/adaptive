
CXX = g++
CXXFLAGS = -std=c++11 -O3 -fopenmp #-DCONFIG_VERBOSE

all: par_for par_adapt test-adpt

par_adapt: par_adapt.cpp 

par_for: par_for.cpp

test-adpt: test-adpt.cpp

clean:
	rm -f par_adapt par_for test-adpt *.o
