timecmp
=======

Timecmp is a utility to compare timestamps that can roll over.
Counter's greatest value must be all ones of any number of bits up to 64.

Compare operations will take overflow situations into account, i.e. a 
less than operation will produce true if the compared value would be reached 
faster by decrementing the counter than incrementing it.

The compare operations are designed to be as optimized as possible.

There is a C-style define implementation as well as a C++-style template implementation.

In C++-style implementation counters of different size are different types. Thus 
the compiler will, rightly so, vomit when attempting to perform operations with
different types.


USAGE (with your application)
-----------------------------

...
