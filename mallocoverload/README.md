mallocoverload
==============

A simple example how to overload malloc and free in C++ code.

To overload malloc in your application without its recompilation:
LD_PRELOAD=/full/path/to/mallocoverload.so ./your_application
