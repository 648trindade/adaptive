
CC = gcc
CXX = g++
CFLAGS = -Wall -O3 -fopenmp -lm
CXXFLAGS = $(CFLAGS) -std=c++11

ifdef DDEBUG
CXXFLAGS += -DDDEBUG
endif

all: test ctest

test: test-adpt.cpp
	$(CXX) test-adpt.cpp $(CXXFLAGS) -o test-adpt

ctest: test-adpt.c
	$(CXX) recursive-for_c_connector.cpp $(CXXFLAGS) -fno-exceptions -c
	$(CC) test-adpt.c  $(CFLAGS) -c 
	$(CC) test-adpt.o recursive-for_c_connector.o $(CFLAGS) -lstdc++ -o test-adapt-c

clean:
	rm -f test-adpt test-adapt-c *.o
