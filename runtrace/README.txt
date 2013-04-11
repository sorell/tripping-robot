
Runtrace facility is for tracing your application's execution. You can place 
tracepoint creation macros in your software and print out the history any time.
Every tracepoint saves line, line or calling function and an optional custom
message.

Runtrace is designed for debugging application where your can't or it's not 
feasible to use printf. It is at its best in performance critical sections,
timing execution flow or tracking multithreading software. 
This facility is multithread safe and works in kernel and in user space.

Creating tracepoint is fast by design: It does not involve memory allocation 
and multiple threads or interrupt contexts can create tracepoints without 
blocking each other.
Printing out trace history is a bit slower and locks the facility for a brief
moment, although not for whole printing procedure.


USAGE (with your application)
=============================

Initialising: 
runtrace_init(): Call when your application starts or before any tracepoints 
are created.

On exit: 
runtrace_exit(): Deallocate any resources allocated by the facility.

Configure: 
runtrace_reconfigure(size): Will resize the tracepoint history buffer with 
'size' objects. Default is 256. Can be called at any time, but will destroy 
existing tracepoint data.

Creating tracepoints: 
TP_FILE(msg), TP_FUNK(msg): Creates a tracepoint with __LINE__ and __FILE__ or 
__FUNCTION__ information. Message pointed by 'msg' is copied to tracepoint 
buffer. The 'msg' pointer can be null.
TP_FILE_P(fmt...), TP_FUNK_P(fmt...): As above but printf-style parameters can
be used.

Printing the tracepoint stack: 
print_trace_stack(flags, dst, size, priv): Will print the tracepoint history to
desired destination. Flags can be used to determine how data is displayed. 
'Dst' may be a pointer to a buffer with 'size' bytes of space. If 'dst' is 
null, stderr (userspace) or printk (kernel) is used.
