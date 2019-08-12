
CXX = clang++
CC = clang
CFLAGS = -Wall -O0 -g -fopenmp -lm
CXXFLAGS = $(CFLAGS) -std=c++11 #-DCONFIG_VERBOSE
LDFLAGS = # -L/home/jvlima/install/libkomp/lib -lomp

all: adpt adpt-rec naive-rec adapt-rec2

adpt:
	$(CXX) example.cpp $(CXXFLAGS) -Iadaptive -o cpp_example_adaptive $(LDFLAGS)
	$(CXX) adaptive/adaptive_c_connector.cpp $(CXXFLAGS) -fno-exceptions -Iadaptive -c
	$(CC) example.c adaptive_c_connector.o $(CFLAGS) -lstdc++ -Iadaptive -o c_example_adaptive

adpt-rec:
	$(CXX) example.cpp $(CXXFLAGS) -Iadaptive-recursive -o cpp_example_adaptive-recursive $(LDFLAGS)
	$(CXX) adaptive-recursive/adaptive_c_connector.cpp $(CXXFLAGS) -fno-exceptions -Iadaptive-recursive -c
	$(CC) example.c adaptive_c_connector.o $(CFLAGS) -lstdc++ -Iadaptive-recursive -o c_example_adaptive-recursive

adapt-rec2:
	$(CXX) example2.cpp $(CXXFLAGS) -Iadaptive-recursive -o cpp_example2_adaptive_rec $(LDFLAGS)

naive-rec:
	$(CXX) example.cpp $(CXXFLAGS) -Inaive-recursive -o cpp_example_naive-recursive $(LDFLAGS)
	$(CXX) naive-recursive/adaptive_c_connector.cpp $(CXXFLAGS) -fno-exceptions -Inaive-recursive -c
	$(CC) example.c adaptive_c_connector.o $(CFLAGS) -lstdc++ -Inaive-recursive -o c_example_naive-recursive

clean:
	rm -rf *.o c_example* cpp_example*
