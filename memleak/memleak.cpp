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

/***** TODO *****************************************************************
 *
 * Nothing right now
 * 
*****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "memleak.h"

#ifdef SS_ENABLE_MEMTRACE

#define SS_TRACE "Memtrace"

#undef malloc
#undef free
#undef new


#ifdef SS_MEMTRACE_THREADSAFE
static pthread_mutex_t rec_lock = PTHREAD_MUTEX_INITIALIZER;

# define ss_pthread_mutex_lock(x)   pthread_mutex_lock(x)
# define ss_pthread_mutex_unlock(x) pthread_mutex_unlock(x)
#else
# define ss_pthread_mutex_lock(x)   do {} while (0)
# define ss_pthread_mutex_unlock(x) do {} while (0)
#endif


unsigned int ss_memleak_tracking = 0;
unsigned int ss_alloc_max_tolerate = SS_ALLOC_MAX_TOLERATE_DEFAULT;


/***** A catch **************************************************************
 *
 * There is a catch in here.
 * The memory allocator system takes a bit of every allocated block for its
 * own use.
 * The main information storage is a sorted linked list where the 
 * bookkeeping is done. When the application frees memory, it needs a pointer
 * to this information to update the information representing the memory 
 * block. This pointer is stored in the head space of every allocated memory 
 * block. This space is called 'piggyback'.
 * 
*****************************************************************************/

// The size of pointer depends on the system. 
// F.ex. i386 32 bit has 4 byte pointer and 64 bit has 8 byte pointer.
// #define PIGGYBACK_SIZE (sizeof(void *))
#define PIGGYBACK_SIZE    12

struct alloc_record;

struct piggyback_data 
{
	alloc_record *record;
	size_t       size;
} __attribute__ ((packed));


// Pointer to integer cast macro
#ifdef __x86_64__
# define PTR_TO_INT_CAST  long long
# define ALLOC_ALIGN      16
#else
# define PTR_TO_INT_CAST  long
# define ALLOC_ALIGN      8
#endif

#define PTR_TO_TRUEPTR(ptr)    (((char *) ptr) - PIGGYBACK_SIZE)
#define TRUEPTR_IS_VALID(ptr)  ((PTR_TO_INT_CAST) true_ptr % ALLOC_ALIGN == 0)


/*
 * I just left the example code for using malloc wrapper

#include <dlfcn.h>

void *(*fpmalloc)(size_t);
static void (*fpfree)(void *);

static void
resolve_malloc(void)
{
	char *error;
	fpmalloc = (void* (*)(size_t)) dlsym(RTLD_NEXT, "malloc");
	if (NULL != (error = dlerror())) {
		fprintf(stderr, "dlsym: %s\n", error);
		exit(1);
	}
	if (NULL == fpmalloc) {
		fprintf(stderr, "fpmalloc = NULL\n");
		exit(1);
	}
}

static void
resolve_free(void)
{
	char *error;
	fpfree = (void (*)(void *)) dlsym(RTLD_NEXT, "free");
	if (NULL != (error = dlerror())) {
		fprintf(stderr, "dlsym: %s\n", error);
		exit(1);
	}
	if (NULL == fpfree) {
		fprintf(stderr, "fpfree = NULL\n");
		exit(1);
	}
}
*/



/****************************************************************************
   LOGGING OPERATIONS
*****************************************************************************/

#ifdef SS_MEMTRACE_LOG_TIMESTAMP
static void
ml_log_timestamp(FILE *const Stream)
{
	time_t Time = time(NULL);
	struct tm *Tm = localtime(&Time);
	
	fprintf(Stream, "[%02d%02d%02d:%02d%02d%02d] ", 
		Tm->tm_year - 100, Tm->tm_mon + 1, Tm->tm_mday,
		Tm->tm_hour, Tm->tm_min, Tm->tm_sec);
}
#endif


/*---- Function -------------------------------------------------------------
  Does: 
    Writes text in the desired output. The output is defined by 
    SS_MEMTRACE_LOG. Stderr is used in case SS_MEMTRACE_LOG = "".
    If SS_MEMTRACE_LOG_TIMESTAMP is defined, timestamp is inserted in every
    line.
  
  Wants:
    Printf-style input.
	
  Gives: 
    Nothing.
----------------------------------------------------------------------------*/

#ifndef SS_MEMTRACE_LOG
# define SS_MEMTRACE_LOG ""
#endif

static void
ml_log(char const *const s, ...)
{
	FILE *LogStream = NULL;
	
	
	if (SS_MEMTRACE_LOG[0] != '\0') {
		LogStream = fopen(SS_MEMTRACE_LOG, "a+");
		
		if (!LogStream)
		{
			fprintf(stderr, "Opening %s for logging failed!\n", SS_MEMTRACE_LOG);
			exit(1);
		}
	}
	else {
		LogStream = stderr;
	}
	
	
#ifdef SS_MEMTRACE_LOG_TIMESTAMP
	static bool LineEnd = 1;
	
	if (!LineEnd) {
		LineEnd = strchr(s, '\n') != NULL;
	}
	else {
		ml_log_timestamp(LogStream);
		LineEnd = strchr(s, '\n') != NULL;
	}
#endif
	
	va_list args;
	va_start(args, s);
	vfprintf(LogStream, s, args);
	va_end(args);
	
	if (SS_MEMTRACE_LOG[0] != '\0') {
		fclose(LogStream);
	}
}


/****************************************************************************
   LIST OPERATIONS
*****************************************************************************/


struct alloc_record
{
	struct alloc_record *next;
	struct alloc_record *prev;
	char const *file;
	int        line;
	int        cnt;
	size_t     mem_total;
	bool       dirty;
	bool       overallocations;
};

struct alloc_list_head
{
	struct alloc_record *next;
	struct alloc_record *prev;
};


static struct alloc_list_head alloc_list =
	{ (struct alloc_record *) &alloc_list, (struct alloc_record *) &alloc_list };

#define end_of_the_list(list) ((struct alloc_record *) &list)

#define list_for_each(iter, list) \
	for (iter = list.next; iter != end_of_the_list(list); iter = iter->next)


/*---- Function -------------------------------------------------------------
  Does: 
    Adds a node in the linked list after the given node.
  
  Wants:
    record - The node to insert.
	head   - The node in list.
	
  Gives: 
    Nothing.
----------------------------------------------------------------------------*/

static inline void
list_add_tail(struct alloc_record *const record, struct alloc_record *const head)
{
	head->prev->next = record;
	record->next     = head;
	record->prev     = head->prev;
	head->prev       = record;
}


/*---- Function -------------------------------------------------------------
  Does: 
    Finds a node related to given file and line. Increments that node's use
	counter by 1. If such a node is not found, inserts a new one in the list.
  
  Wants:
    file - The name of the file where memory allocation occured.
	line - The line, likewise.
	
  Gives: 
    Pointer to the node of given file and line.
----------------------------------------------------------------------------*/

static struct alloc_record *
list_push_record(char const *const file, int const line, size_t const size, bool const overalloc)
{
	struct alloc_record *list_it;
	int cmp_result;

	ss_pthread_mutex_lock(&rec_lock);
	
	list_for_each(list_it, alloc_list)
	{
		cmp_result = strcmp(list_it->file, file);
		
		if (cmp_result > 0) {
			break;
		}
		if (0 == cmp_result  &&  list_it->line > line) {
			break;
		}
		if (0 == cmp_result  &&  line == list_it->line) {
			
			if (size > 0) {
				++list_it->cnt;
				list_it->mem_total += size;
			
				if (overalloc) {
					list_it->overallocations = true;
				}
			}
			
			list_it->dirty = true;
			
			ss_pthread_mutex_unlock(&rec_lock);
			return list_it;
		}
	}
	
	struct alloc_record *const record = (struct alloc_record *) malloc(sizeof(struct alloc_record));
	
	if (!record) {
		ml_log(SS_TRACE ": Failed to allocate memory for list\n");
		exit(1);
	}
	
	record->file      = file;
	record->line      = line;
	record->cnt       = size > 0 ? 1 : 0;
	record->mem_total = size;
	record->dirty     = true;
	record->overallocations = false;
	
	if (overalloc) {
		record->overallocations = true;
	}
	
	list_add_tail(record, list_it);
	
	ss_pthread_mutex_unlock(&rec_lock);
	
	return record;
}


struct alloc_record *
ss_memleak_point_register(char const *const file, int const line)
{
	return list_push_record(file, line, 0, false);
}


/****************************************************************************
   REPORTING OPERATIONS
*****************************************************************************/

#ifdef SS_ENABLE_MEMTRACE_EXIT

class RecordExitChecker
{
public:
	RecordExitChecker() { }
	~RecordExitChecker()
	{
		struct alloc_record *list_it;
		
		ml_log(SS_TRACE ": Application exited\n");
		ss_pthread_mutex_lock(&rec_lock);
		
		list_for_each(list_it, alloc_list)
		{
			ml_log(SS_TRACE ": %d unclean records from %s: %d  %s\n", list_it->cnt, list_it->file, list_it->line,
				list_it->overallocations ? "(OA)" : "");
		}
		
		ss_pthread_mutex_unlock(&rec_lock);
	}
};


static RecordExitChecker ExitChecker;

#endif  // SS_ENABLE_MEMTRACE_EXIT


/*---- Function -------------------------------------------------------------
  Does: 
    Prints out the current status of alloated memory blocks.
  
  Wants:
    flags
	  MEML_TIGHT - All counters in one line.
	  MEML_SUPPRESS_ZEROS - Do not display zero counters.
	  MEML_ONLY_CHANGED - Print only changed records.
	
  Gives: 
    Nothing.
----------------------------------------------------------------------------*/

void
memleak_report(int const flags)
{
	const bool tight     = flags & MEML_TIGHT;
	const bool suppress0 = flags & MEML_SUPPRESS_ZEROS;
	const bool changed   = flags & MEML_ONLY_CHANGED;
	struct alloc_record *list_it;
	
	
	if (!tight)  ml_log(SS_TRACE ": %s\n", __FUNCTION__);
	if (tight)   ml_log(SS_TRACE ": ");
	
	ss_pthread_mutex_lock(&rec_lock);
	
	list_for_each(list_it, alloc_list)
	{
		if (changed  &&  !list_it->dirty) {
			continue;
		}
		
		if (suppress0  &&  0 == list_it->cnt) {
			continue;
		}
		
		if (!tight) {
			ml_log(SS_TRACE ": %4d records, %7lu bytes from line %s: %d  %s\n", list_it->cnt, list_it->mem_total, list_it->file, list_it->line,
				list_it->overallocations ? "(OA)" : "");
		}
		else {
			ml_log("%d ", list_it->cnt);
		}
		
		list_it->dirty = false;
	}
	
	ss_pthread_mutex_unlock(&rec_lock);
	
	if (tight) ml_log("\n");
}


/*---- Function -------------------------------------------------------------
  Does: 
    Searches the number of active allocations, made by memleak facility, made 
	from the same line of code as the given ptr.
  
  Wants:
    ptr
	  A pointer of a memory allocated by memleak facility.
	
  Gives: 
    > 0
	  The number of listed active allocations.
	-1
	  The ptr was invalid, ie. not listed by memleak facility.
----------------------------------------------------------------------------*/

int 
memleak_allocs_at(void const *const ptr)
{
	// Get the original allocated address
	// void *const true_ptr = ((char *) ptr) - PIGGYBACK_SIZE;
	void const *const true_ptr = PTR_TO_TRUEPTR(ptr);
	
	// If the calculated 'true pointer' is divisible by 8 (on 32 bit system) 
	// or by 16 (on 64 bit system), then this pointer is one mangled by us
	// if ((PTR_TO_INT_CAST) true_ptr % ALLOC_ALIGN == 0) {
	if (!TRUEPTR_IS_VALID(true_ptr)) {
		return -1;
	}
	
	struct piggyback_data const *const pbdata = (struct piggyback_data *) true_ptr;
	struct alloc_record const *const record = pbdata->record;
	
	return record->cnt;
}


/****************************************************************************
   MEMORY ALLOCATION
*****************************************************************************/


/*---- Function -------------------------------------------------------------
  Does: 
    Allocates memory block whose size is according to application's desire +
	the size of piggyback block. Updates the bookkeeping and puts the list
	node's pointer in the head of the allocated block.
  
  Wants:
    size - The amount of memory the application wants.
	file - From where it wants it from.
	line - From where it wants it from.
	
  Gives: 
    Pointer to the allocated memory shifted by the size of piggyback block.
----------------------------------------------------------------------------*/

static void *
get_memory(size_t const size, char const *const file, int const line)
{
	void *const ptr = malloc(PIGGYBACK_SIZE + size);
	bool const overalloc = ss_alloc_max_tolerate > 0  &&  size > ss_alloc_max_tolerate;
	
	if (overalloc) {
		ml_log(SS_TRACE ": Allocation size (%u) exceeded tolerance level at %s: %d\n", size, file, line);
	}
	
	if (!ptr) {
		ml_log(SS_TRACE ": Could not allocate memory at %s: %d\n", file, line);
		return NULL;
	}
	
	struct alloc_record *const record = list_push_record(file, line, size, overalloc);
	
	// Store the record's pointer in the head of the allocated memory area
	struct piggyback_data *const pbdata = (struct piggyback_data *) ptr;
	pbdata->record = record;
	pbdata->size   = size;
	
	// Shift the allocated memory area's starting address by the size
	// of the piggybacked area
	return ((char *) ptr) + PIGGYBACK_SIZE;
}


/*---- Function -------------------------------------------------------------
  Does: 
    Checks if the given pointer is allocated by bookkeeper. (This can be 
	deducted from the alignment.)
	Updates bookkeeping and frees the memory.
	The memory is freed even when it's not originally alloated by the book-
	keeper process.
  
  Wants:
    ptr - The application's view of the memory it wants freed.
	
  Gives: 
    True - The memory was originally allocated by the bookkeeper.
	False - The memory was _not_ originally ...
----------------------------------------------------------------------------*/

static bool
free_memory(void *const ptr)
{
	// Get the original allocated address
	void *const true_ptr = ((char *) ptr) - PIGGYBACK_SIZE;
	
	// If the calculated 'true pointer' is divisible by 8 (on 32 bit system) 
	// or by 16 (on 64 bit system), then this pointer is one mangled by us
	if ((PTR_TO_INT_CAST) true_ptr % ALLOC_ALIGN == 0) {
		struct piggyback_data *const pbdata = (struct piggyback_data *) true_ptr;
		struct alloc_record *const record = pbdata->record;
		
		ss_pthread_mutex_lock(&rec_lock);
		
		--record->cnt;
		record->dirty = true;
		record->mem_total -= pbdata->size;
		
		ss_pthread_mutex_unlock(&rec_lock);
		
		free(true_ptr);
		
		return true;
	}
	
	// This memory was not allocated by us, but we need to free it anyway
	free(ptr);
	return false;
}


/****************************************************************************
   REPLACEMENT ALLOCATORS
*****************************************************************************/

void *
ss_malloc(size_t const size, char const *const file, int const line)
{
	if (!ss_memleak_tracking) {
		return malloc(size);
	}
#ifdef SS_MEMTRACE_VERBOSE
	ml_log(SS_TRACE ": malloc(%d) from %s: %d\n", size, file, line);
#endif
	
	// void *ptr = get_memory(size, file, line);
	// return ptr;
	return get_memory(size, file, line);
}

void
ss_free(void *ptr)
{
	const bool alloc_by_us = free_memory(ptr);
#ifdef SS_MEMTRACE_VERBOSE
	if (alloc_by_us) {
		ml_log(SS_TRACE ": free(%p)\n", ptr);
	}
#endif
	(void) alloc_by_us;
}

void* 
operator new (size_t const size, ss_new_t, char const *const file, int const line)
{
	if (!ss_memleak_tracking) {
		return malloc(size);
	}
#ifdef SS_MEMTRACE_VERBOSE
	ml_log(SS_TRACE ": new(%d) from %s: %d\n", size, file, line);
#endif
	
	// void *ptr = get_memory(size, file, line);
	// return ptr;
	return get_memory(size, file, line);
}

void
operator delete (void *const ptr)
{
	const bool alloc_by_us = free_memory(ptr);
#ifdef SS_MEMTRACE_VERBOSE
	if (alloc_by_us) {
		ml_log(SS_TRACE ": delete(%p)\n", ptr);
	}
#endif
	(void) alloc_by_us;
}

void *
operator new [] (size_t const size, ss_new_t, char const *const file, int const line)
{
	if (!ss_memleak_tracking) {
		return malloc(size);
	}
#ifdef SS_MEMTRACE_VERBOSE
	ml_log(SS_TRACE ": new[](%d) from %s: %d\n", size, file, line);
#endif
	
	// void *ptr = get_memory(size, file, line);
	// return ptr;
	return get_memory(size, file, line);
}

void
operator delete [] (void *const ptr)
{
	const bool alloc_by_us = free_memory(ptr);
#ifdef SS_MEMTRACE_VERBOSE
	if (alloc_by_us) {
		ml_log(SS_TRACE ": delete[](%p)\n", ptr);
	}
#endif
	(void) alloc_by_us;
}

#endif  // SS_ENABLE_MEMTRACE
