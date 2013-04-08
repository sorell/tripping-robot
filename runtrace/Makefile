
LINUXPATH=/usr/src/linux/

rt_mod_test-objs:=runtrace.o test/mod_test.o
obj-m:=rt_mod_test.o


APP_OPTIONS:=-Wall -O2 -std=c++11
MOD_OPTIONS:=-Wall -O2
LIBS:=-lpthread


all: lib test_m test_a

lib:
	$(CXX) $(APP_OPTIONS) -c runtrace.c 
	$(AR) rvs runtrace.a runtrace.o

clean_lib:
	rm -f runtrace.a runtrace.o

test_a: lib
	$(CXX) $(APP_OPTIONS) -o app_test test/app_test.c runtrace.a $(LIBS)

clean_test_a:
	rm -f app_test

test_m:
	make -C $(LINUXPATH) SUBDIRS=`pwd` modules
	$(CXX) $(APP_OPTIONS) -o mod_test_app test/mod_test_app.c $(LIBS)

clean_test_m:
	make -C $(LINUXPATH) SUBDIRS=`pwd` clean
	rm -f mod_test_app

clean: clean_lib clean_test_m clean_test_a
