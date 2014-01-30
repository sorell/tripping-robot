memleak
=======

Memleak is a utility to keep track of memory allocation and deallocation. This tool
is written to replace dmalloc where it can not or is not feasible to use in.
Memleak comes with fewer features and a lot smaller CPU load.

I suggest use rather use dmalloc if your platform can afford the CPU load hit and
you need its features.


USAGE
-----
Integration with your app:
1) Compile as a shared library by 'make lib' and -l it in your application. You
  also need to #include memleak.h in your source code files you wish to track.

2) Alternatively you can compile memleak.cpp with your application.

NOTE! Be sure to set ss_memleak_tracking=1 in your application start.

Generating statistics:
At any time in your application you can call memleak_report()
See flags defined in memleak.h for formatting the output.


CONFIGURATION
-------------
Configuration is done by #define directives in memleak.h.

SS_ENABLE_MEMTRACE
  When commented out memleak does not have any impact on your application.

SS_ENABLE_MEMTRACE_EXIT
  Normal application termination prints out memleak report.
  Do note that application termination by signal does not call this. For this
  implement a signal handler in your app. See an example in test.cpp.

SS_MEMTRACE_VERBOSE
  Causes every allocation and free operation to print out a text.

SS_MEMTRACE_LOG
  Memleak output destination. Must be a valid path/filename or "" for stderr.

SS_MEMTRACE_LOG_TIMESTAMP
  Add timestamp to printed lines.

SS_MEMTRACE_THREADSAFE
  Enable for multithreading applications.

