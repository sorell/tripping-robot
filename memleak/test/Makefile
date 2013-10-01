
CC:=$(CC_PREFIX)cc
CXX:=$(CC_PREFIX)g++

all:
	$(CXX) -O2 -Wall -o test test.cpp -L.. -lmemleak -lpthread
	
clean:
	rm -f test
