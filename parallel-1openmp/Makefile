CC	= g++
CFLAGS	= -std=c++11 -O3 -fopenmp

all: logVS

logVS: main.o
	$(CC) -o $@ $^ $(CFLAGS)

main.o: main.cpp
	$(CC) -c $(CFLAGS) $<

.PHONY: clean

clean: 
	rm -f *.o
	rm -f logVS
