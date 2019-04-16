
CC = gcc
CFLAGS = -Wall -O3 -fopenmp -lm

all: test

test: test-adpt.c lib
	$(CC) test-adpt.c adaptive.o $(CFLAGS) -lm -o test-adpt

lib: adaptive.c
	$(CC) -c adaptive.c $(CFLAGS)

clean:
	rm -f teste-adpt *.o
