
CXX = g++
CC = gcc
CFLAGS = -Wall -O3 -fopenmp -lm
CXXFLAGS = $(CFLAGS) -std=c++11 #-DCONFIG_VERBOSE

all: test-adpt-c par_for par_adapt test-adpt

test-adpt-c:
	$(CXX) parallel_for_c_connector.cpp $(CXXFLAGS) -fno-exceptions -c
	$(CC) test-adpt.c  $(CFLAGS) -c 
	$(CC) test-adpt.o parallel_for_c_connector.o $(CFLAGS) -lstdc++ -o test-adpt-c

par_adapt: par_adapt.cpp 

par_for: par_for.cpp

test-adpt: test-adpt.cpp
	$(CXX) test-adpt.cpp $(CXXFLAGS) -o test-adpt

clean:
	rm -f par_adapt par_for test-adpt test-adpt-c *.o
