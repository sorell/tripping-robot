/*
 * Copyright (C) 2009-2013 Sami Sorell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef STOPWATCH_H_HEADER
#define STOPWATCH_H_HEADER

/*
 * Usage:
 * 
 * STOPWATCH_INIT(A);
 * STOPWATCH_START(A);
 *
 * // Do you stuff here
 *
 * STOPWATCH_STOP(A);
 * STOPWATCH_PRINT(A);
 * 
 * double diff = STOPWATCH_ELAPSED_F(A);
 
 * int secs  = STOPWATCH_ELAPSED_S(A);
 * int usecs = STOPWATCH_ELAPSED_US(A);
*/

#ifdef __KERNEL__
# include <linux/time.h>
#else
# include <sys/time.h>
#endif


struct __stopwatch_priv {
	struct timeval start;
	struct timeval stop;
};


#define DTIMER_PRINT_PARAMS(x) \
	(__stopwatch_priv_##x.start.tv_usec > __stopwatch_priv_##x.stop.tv_usec ? \
	__stopwatch_priv_##x.stop.tv_sec - __stopwatch_priv_##x.start.tv_sec - 1 : \
	__stopwatch_priv_##x.stop.tv_sec - __stopwatch_priv_##x.start.tv_sec), \
	(__stopwatch_priv_##x.start.tv_usec > __stopwatch_priv_##x.stop.tv_usec ? \
	__stopwatch_priv_##x.stop.tv_usec + 1000000 - __stopwatch_priv_##x.start.tv_usec : \
	__stopwatch_priv_##x.stop.tv_usec - __stopwatch_priv_##x.start.tv_usec)


#ifdef __KERNEL__
# define __STOPWATCH_GETTIMEOFDAY(x)  do_gettimeofday(&x)
# define _STOPWATCH_PRINT(x, fmt)  printk(KERN_WARNING fmt, DTIMER_PRINT_PARAMS(x))
#else  // ! __KERNEL__
# define __STOPWATCH_GETTIMEOFDAY(x)  gettimeofday(&x, NULL)
# define _STOPWATCH_PRINT(x, fmt)  fprintf(stderr, fmt, DTIMER_PRINT_PARAMS(x))
# define _STOPWATCH_PRINT_FILE(file, x, fmt)  fprintf(file, fmt, DTIMER_PRINT_PARAMS(x))
#endif


#define STOPWATCH_INIT(x) \
	static __stopwatch_priv __stopwatch_priv_##x = {{0, 0}, {0, 0}}


#define STOPWATCH_START(x) \
	do { __STOPWATCH_GETTIMEOFDAY(__stopwatch_priv_##x.start); } while(0)


#define STOPWATCH_STOP(x) \
	do { __STOPWATCH_GETTIMEOFDAY(__stopwatch_priv_##x.stop); } while (0)


#define STOPWATCH_ELAPSED_S(x) \
	(__stopwatch_priv_##x.start.tv_usec > __stopwatch_priv_##x.stop.tv_usec ? \
	__stopwatch_priv_##x.stop.tv_sec - __stopwatch_priv_##x.start.tv_sec - 1 : \
	__stopwatch_priv_##x.stop.tv_sec - __stopwatch_priv_##x.start.tv_sec)


#define STOPWATCH_ELAPSED_US(x) \
	(__stopwatch_priv_##x.start.tv_usec > __stopwatch_priv_##x.stop.tv_usec ? \
	__stopwatch_priv_##x.stop.tv_usec + 1000000 - __stopwatch_priv_##x.start.tv_usec : \
	__stopwatch_priv_##x.stop.tv_usec - __stopwatch_priv_##x.start.tv_usec)


#define STOPWATCH_ELAPSED_F(x) \
	((double)__stopwatch_priv_##x.stop.tv_sec - (double)__stopwatch_priv_##x.start.tv_sec + \
	(((double)__stopwatch_priv_##x.stop.tv_usec - (double)__stopwatch_priv_##x.start.tv_usec) / 1000000))


#define STOPWATCH_PRINT(x) _STOPWATCH_PRINT(x, "Stopwatch " #x ": %ld.%06ld sec\n");

#ifndef __KERNEL__
# define STOPWATCH_PRINT_FILE(x, file) _STOPWATCH_PRINT_FILE(file, x, "Stopwatch " #x ": %ld.%06ld sec\n");
#endif

#endif
