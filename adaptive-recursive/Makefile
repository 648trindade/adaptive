
CXX = g++
CC = gcc
CFLAGS = -Wall -O3 -fopenmp -lm
CXXFLAGS = $(CFLAGS) -std=c++11 #-DCONFIG_VERBOSE

all: par_for par_adapt

par_adapt: par_adapt.cpp 

par_for: par_for.cpp

clean:
	rm -f par_adapt par_for test-adpt test-adpt-c *.o
