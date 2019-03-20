LIBNAME=mallocoverload

lib:
	$(CROSS_COMPILE)g++ -Wall -O2 -fPIC -c $(LIBNAME).c
	$(CROSS_COMPILE)ar rcs $(LIBNAME).a $(LIBNAME).o
	$(CROSS_COMPILE)g++ -shared $(LIBNAME).o -o $(LIBNAME).so -lc -ldl

test: lib
	$(CROSS_COMPILE)g++ -Wall -O2 -g -o test_$(LIBNAME) test.c

testrun: test
	LD_PRELOAD=$(PWD)/$(LIBNAME).so ./test_$(LIBNAME)

clean:
	rm -f $(LIBNAME).o $(LIBNAME).a $(LIBNAME).so test_$(LIBNAME)
