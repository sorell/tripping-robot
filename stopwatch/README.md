stopwatch
=========

Stopwatch is a very simple tool for tracking time.
All code resides in one header file, which has convenience macros for 
operations.


USAGE (with your application)
-----------------------------

Initialising: 
STOPWATCH_INIT(your_timer_name)
- Wrapper to create a static struct to keep the timer information.

STOPWATCH_START(ytn)
- Writes down the starting time. A new call to this macro will overwrite the
previous time you may have recorded.

STOPWATCH_STOP(ytn)
- As _START, but manipulates the stopping timestamp.

STOPWATCH_ELAPSED_S(ytn)
STOPWATCH_ELAPSED_US(ytn)
- Returns the seconds part (_S) or microseconds part (_US) of the difference 
between start and stop timestamps in integer value.

STOPWATCH_ELAPSED_F(ytn)
- Returns the difference between start and stop timestamps in floating point
value.

STOPWATCH_TO_TIMEVAL(your_timeval, ytn)
- Copies difference between start and stop timestamps to timeval struct.

STOPWATCH_PRINT(ytn)
- Prints the difference between start and stop timestamps out to stderr 
(userspace) or to prink buffer (kernel, KERN_WARNING level).

STOPWATCH_PRINT_FILE(ytn, file_handle)
- Prints the same information to a file instead. (stdout, stderr, or any opened
file). Do notice that fileno(FILE *stream) returns the file descriptor of a 
file opened with fopen.

