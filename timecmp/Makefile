
PROJ = timecmp-test
SRCS = test.cpp test-c.c test-cpp.cpp


all:
	g++ -Wall -std=c++11 -g -Wa,-a,-ad -O2 -o $(PROJ) $(SRCS) > .asm

clean:
	rm -f $(PROJ)
