
PROJ = obscurestr-test
SRCS = test.cpp


.PHONY: all clean test

all:
	g++ -Wall -std=c++14 -g -Wa,-a,-ad -O2 -o $(PROJ) $(SRCS) > .asm

clean:
	rm -f $(PROJ)

test: all
	@# Remove debug information to banish obscured strings from the binary
	@strip -s obscurestr-test
	@# Show that they're actually gone
	@! (strings obscurestr-test | grep ObscureStr)
	./obscurestr-test
