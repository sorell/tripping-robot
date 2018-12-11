timecmp
=======

Timecmp is a utility to compare timestamps that can roll over.
The timer variable can be of any size from (theoretical) 1 bit up to 64 bits, and
its greatest value's bits must be all ones. I.e. this implementation doesn't
support timers that roll over after f.ex. 0xF80.

Compare operations will take overflow situations into account.
A less than operation will produce true if the compared value would be reached 
faster by decrementing the timer than incrementing it.

The compare operations are designed to be as cpu optimal as possible.

There is a C-style inline implementation as well as a C++-style template implementation.

In C++-style implementation timers of different size are different types. Thus 
the compiler will, rightly so, vomit when attempting to perform operations with
different types.


USAGE (with your application)
-----------------------------

C-style:
--------
```cpp
#define COUNTER_MASK 0x7FFF
u16 timer1 = 0x7000;
u16 timer2 = (timer1 + 0x1000) & COUNTER_MASK;
timeIsAfter(timer2, timer1, COUNTER_MASK);  // Is true
```

C++-style:
----------
```cpp
RollingTime<15> timer1(0x7000);
RollingTime<15> timer2(timer1);
timer2 += 0x1000;
timer2 > timer1;  // Is true
```
