tripping-robot
==============

This is a repository containing nifty little utilities I've written to fit in
various debugging purposes. These are designed for my personal needs and their
level of configuration possibilities and documentation is not on par with well
maintained apps. However, I distribute them for free in hopes that somebody 
finds them beneficial.

mallocoverload
--------------
A simple example how to overload malloc and free in C++ code.

memleak
-------
Memleak is a utility to keep track of memory allocation and deallocation. This
tool is written to replace dmalloc where it can't or is not feasible to use in.
Memleak comes with fewer features and a lot smaller CPU load.

obscurestr
----------
Let compiler obscure text strings so they're hard to trace from binary file.
A header-only implementation. Requires c++14.

runtrace
--------
Runtrace facility is for tracing your application's execution. You can place 
tracepoint creation macros in your software and print out the history at any
given time. 
This is designed for debugging applications when your can't or it's not 
feasible to use printf or a debugger. It is at its best in performance critical
sections, timing execution flow or tracking multithreading software. 
Runtrace is multithread safe and works in Linux kernel and in user space.

stopwatch
---------
Stopwatch is a very simple tool for tracking time.
All code resides in one header file, which has convenience macros for 
operations.

timecmp
-------
Timecmp is a utility to compare timestamps that can roll over.
There're C-style and C++-style implementations. Supports timers uneven to 8 bits.

wait4crash
----------
A simple shell script that repeatedly starts and kills an application until it crashes.
