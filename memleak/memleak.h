/*
 * Copyright (C) 2010-2012 Sami Sorell
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


#ifndef SS_MEMLEAK_H
#define SS_MEMLEAK_H

// Comment this to disable all memory tracking
#define SS_ENABLE_MEMTRACE

// Comment this to disable deallocation checking on application exit
//#define SS_ENABLE_MEMTRACE_EXIT

// Comment this to disable per-allocation reporting
// #define SS_MEMTRACE_VERBOSE

// Define as "" to log to stderr
//#define SS_MEMTRACE_LOG "/tmp/memtrace.log"
#define SS_MEMTRACE_LOG "/tmp/scsd-memtrace"

// Comment to leave timestamps out of the log output
// #define SS_MEMTRACE_LOG_TIMESTAMP

// Pretty self-explanatory
#define SS_MEMTRACE_THREADSAFE

// You must set this to != 0 when you want tracking enabled!
extern unsigned int ss_memleak_tracking;

// How big allocation is tolerated to not directly report it (0 = no limit)
#define SS_ALLOC_MAX_TOLERATE_DEFAULT    0
extern unsigned int ss_alloc_max_tolerate;


#ifdef SS_ENABLE_MEMTRACE

// extern const char *ss_memleak_file;
// extern int ss_memleak_line;

#define MEML_DEFAULT         0
#define MEML_TIGHT           0x1
#define MEML_SUPPRESS_ZEROS  0x2
#define MEML_ONLY_CHANGED    0x4

void memleak_report(int flags = MEML_DEFAULT);

// Returns the number of active allocations made from the same line
// as the ptr
int  memleak_allocs_at(void const *ptr);

struct ss_new_t {};
extern struct ss_new_t ss_new;

struct alloc_record;

struct alloc_record *ss_memleak_point_register(char const *file, int line);

void *ss_malloc(size_t const size, char const *file, int line);
void ss_free(void *ptr);

void *operator new (size_t size, ss_new_t, char const *file, int line);
void operator delete (void *ptr);

void *operator new [] (size_t size, ss_new_t, char const *file, int line);
void operator delete [] (void *ptr);

/***** NOTE *****************************************************************
 *
 * I have not yet found out how placement new could be replaced by a custom
 * allocator. You could leave them uncounted, as the memory is allocated 
 * elsewhere. Other option is to manually replace every placement 
 * allocation.
 *
*****************************************************************************/

#endif  // SS_ENABLE_MEMTRACE

#endif  // SS_MEMLEAK_H


#ifndef SS_MEMLEAK_MACROS
#define SS_MEMLEAK_MACROS

#ifdef SS_ENABLE_MEMTRACE

#define malloc(size) ss_malloc(size, __FILE__, __LINE__)
	
#define free(ptr)    ss_free(ptr)

#define new new (ss_new, __FILE__, __LINE__)

#endif  // SS_ENABLE_MEMTRACE

#endif  // SS_MEMLEAK_MACROS
