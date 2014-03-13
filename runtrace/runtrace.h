/*
 * Copyright (C) 2013 Sami Sorell
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

#ifndef RUNTRACE_H_HEADER
#define RUNTRACE_H_HEADER


#define RT_POOL_SIZE_DFLT   256
#define RT_MSG_MAX          200


/**
 * Access macros for recording tracepoints.
 *
 * If you're really worried about CPU cycles when creating a tracepoint, you can avoid 
 * using macros ending in _P. Those macros call vsprintf, ie your message is first printed
 * and then copied again.
 * Macros without _P just do normal memcpy for the message string.
*/

#define TP_FILE(msg)  make_tracepoint(__LINE__, __FILE__, msg)
#define TP_FUNK(msg)  make_tracepoint(__LINE__, __FUNCTION__, msg)
#define TP_FILE_P(fmt...)  tpprintf(__LINE__, __FILE__, fmt)
#define TP_FUNK_P(fmt...)  tpprintf(__LINE__, __FUNCTION__, fmt)


/**
 * Printing flags.
 * Used with print_trace_stack() flags argument.
*/

#define RT_PRINT_TIME    0x01  // Print tracepoint creation timestamp
#define RT_PRINT_DIFF    0x02  // Print current tracepoint's time difference to previous tracepoint
#define RT_PRINT_LINE    0x04  // Print line from where the tracepoint was created
#define RT_PRINT_SRC     0x08  // Print source file/function from where the tracepoint was created
#define RT_PRINT_TIME_MS 0x10  // Print times in ms instead of us

#define RT_PRINT_DEFAULT   (RT_PRINT_TIME | RT_PRINT_LINE | RT_PRINT_SRC | RT_PRINT_TIME_MS)
#define RT_PRINT_ALL       ((~0) & (~RT_PRINT_TIME_MS))
#define RT_PRINT_ALL_MS    (~0)


void make_tracepoint(int line, char const *src, char const *msg);
int  tpprintf(int line, char const *src, char const *fmt, ...);


#ifndef __KERNEL__
void runtrace_flush_on_abort(int enable, int print_flags = RT_PRINT_DEFAULT);
#endif

int  runtrace_reconfigure(int nr_threads, int pool_size);

int  runtrace_init(int nr_threads);
void runtrace_exit(void);

#ifdef __KERNEL__
 int print_trace_stack(int flags, char *dst, int size, int *priv);
 #define printk_trace_stack(f)  print_trace_stack(f, NULL, 0, NULL)
#else
 int print_trace_stack(int flags = RT_PRINT_DEFAULT, char *dst = NULL, int size = 0, int *priv = NULL);
#endif

#endif
