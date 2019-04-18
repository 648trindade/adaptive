
CXX = g++
CXXFLAGS = -Wall -O3 -fopenmp -lm -std=c++11

all: test

test: test-adpt.cpp
	$(CXX) test-adpt.cpp $(CXXFLAGS) -o test-adpt

clean:
	rm -f teste-adpt *.o
