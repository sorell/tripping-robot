
Runtrace facility is for recording custom tracepoints. It works in kernel and
user space, and is multitread safe.

USAGE (with your application)
=============================

Initialising: runtrace_init()
  Call when your application starts or before any tracepoints are created.

On exit: runtrace_exit()
  Deallocate any resources allocated by the facility.

Configure: runtrace_reconfigure(size) 
  Will resize the tracepoint buffer with 'size' objects. Default is 256. Can be
  called at any time, but will destroy existing tracepoint data.

Creating tracepoints: 
TP_FILE(msg), TP_FUNK(msg): Creates a tracepoint with __LINE__ and __FILE__ or 
  __FUNCTION__ information. Message pointed by 'msg' is copied to tracepoint
  buffer. The 'msg' pointer can be null.
TP_FILE_P(fmt...), TP_FUNK_P(fmt...): As above but printf-style parameters can
  be used.

Printing the tracepoint stack: print_trace_stack(flags, dst, size, priv)
  Will print the tracepoint stack to desired destination.
  Flags can be used to determine what data to display. 'Dst' may be a pointer 
  to a buffer with 'size' bytes of space. If 'dst' is null, stderr (userspace)
  or printk (kernel) is used.
