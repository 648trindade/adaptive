
CXX = g++
CC = gcc
CFLAGS = -Wall -O2 -g -fopenmp -lm
CXXFLAGS = $(CFLAGS) -std=c++11 #-DCONFIG_VERBOSE

all: adpt adpt-rec naive-rec

adpt:
	$(CXX) example.cpp $(CXXFLAGS) -Iadaptive -o cpp_example_adaptive
	$(CXX) adaptive/adaptive_c_connector.cpp $(CXXFLAGS) -fno-exceptions -Iadaptive -c
	$(CC) example.c adaptive_c_connector.o $(CFLAGS) -lstdc++ -Iadaptive -o c_example_adaptive

adpt-rec:
	$(CXX) example.cpp $(CXXFLAGS) -Iadaptive-recursive -o cpp_example_adaptive-recursive
	$(CXX) adaptive-recursive/adaptive_c_connector.cpp $(CXXFLAGS) -fno-exceptions -Iadaptive-recursive -c
	$(CC) example.c adaptive_c_connector.o $(CFLAGS) -lstdc++ -Iadaptive-recursive -o c_example_adaptive-recursive

naive-rec:
	$(CXX) example.cpp $(CXXFLAGS) -Inaive-recursive -o cpp_example_naive-recursive
	$(CXX) naive-recursive/adaptive_c_connector.cpp $(CXXFLAGS) -fno-exceptions -Inaive-recursive -c
	$(CC) example.c adaptive_c_connector.o $(CFLAGS) -lstdc++ -Inaive-recursive -o c_example_naive-recursive

clean:
	rm -rf *.o c_example* cpp_example*
