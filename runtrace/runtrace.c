/*
 * Copyright (C) 2013-2014 Sami Sorell
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


/** TODO
 *
 * - Blocking read for dev file?
 * - Configurable message size?
 * - Automated tests for:
 *   - Reconfigure while running (changing pool size)
 *   - different buffer sizes
 *   - timestamp and/or sequence number checking
 *
*/


#ifdef __KERNEL__
 #ifdef MODULE
  #include <linux/module.h>
 #endif

 #include <linux/kernel.h>
 #include <linux/slab.h>
 #include <linux/time.h>
 #include <linux/sched.h>
 #include <linux/fs.h>
 #include <linux/cdev.h>
 #include <asm/atomic.h>
 #include <asm/uaccess.h>
#else
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdarg.h>
 #include <unistd.h>
 #include <sys/time.h>
 #include <signal.h>
 #include <pthread.h>
 
 #ifdef __GXX_EXPERIMENTAL_CXX0X__
  #include <atomic>
 #endif

#endif  // ! __KERNEL__

#include "runtrace.h"


/**
 * Convenience macros for keeping the actual code clean of define pollution:
 * ifdef __KERNEL__
 *     Kernel specific code
 * else
 *     Userspace specific code
 * endif
 *
*/

#ifdef __KERNEL__

 #define MALLOC(x)               kmalloc(x, GFP_KERNEL)
 #define FREE(x)                 kfree(x)
 #define GETTIMEOFDAY(x)         do_gettimeofday(x)
 // #define NR_CPUS  This is defined in kernel headers
 
 #define LOCK_DECLARE(x)         DEFINE_RWLOCK(x)
 #define LOCK_INIT(x)            do {} while(0);
 #define LOCK_DESTROY(x)         do {} while(0);
 #define RDLOCK(x)               read_lock(&x)
 #define WRLOCK(x)               write_lock(&x)
 #define RDUNLOCK(x)             read_unlock(&x)
 #define WRUNLOCK(x)             write_unlock(&x)

 #define ATOMIC_VAR(n)           atomic_t n
 #define ATOMIC_SET(x, n)        atomic_set(&x, n)
 #define ATOMIC_READ(x)          atomic_read(&x)
 #define ATOMIC_RET_INC(x)       (atomic_inc_return(&x) - 1)
 #define ATOMIC_DEC(x)           atomic_dec(&x)

 #define ATOMIC_FLAG(n)          long int n
 #define ATOMIC_TEST_AND_SET(x)  test_and_set_bit(0, &x)
 #define ATOMIC_CLEAR(x)         clear_bit(0, &x)

#else  // ! __KERNEL__

 #define MALLOC(x)               malloc(x)
 #define FREE(x)                 free(x)
 #define GETTIMEOFDAY(x)         gettimeofday(x, NULL)
 #define NR_CPUS                 sysconf(_SC_NPROCESSORS_ONLN)
 #define u64                     unsigned long long

 #define LOCK_DECLARE(x)         pthread_rwlock_t x;
 #define LOCK_INIT(x)            { pthread_rwlockattr_t x##_attr; \
                                 pthread_rwlockattr_init(&x##_attr); \
                                 pthread_rwlockattr_setkind_np(&x##_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP); \
                                 pthread_rwlock_init(&x, &x##_attr); \
                                 pthread_rwlockattr_destroy(&x##_attr); }
 #define LOCK_DESTROY(x)         pthread_rwlock_destroy(&x)
 #define RDLOCK(x)               pthread_rwlock_rdlock(&x)
 #define WRLOCK(x)               pthread_rwlock_wrlock(&x)
 #define RDUNLOCK(x)             pthread_rwlock_unlock(&x)
 #define WRUNLOCK(x)             pthread_rwlock_unlock(&x)

 #if defined __cplusplus  &&  defined __GXX_EXPERIMENTAL_CXX0X__
  #define ATOMIC_VAR(n)           std::atomic<unsigned int> n
  #define ATOMIC_SET(x, n)        x = n
  #define ATOMIC_READ(x)          x
  #define ATOMIC_RET_INC(x)       x.fetch_add(1)

  #define ATOMIC_FLAG(n)          std::atomic_flag n
  #define ATOMIC_TEST_AND_SET(x)  x.test_and_set()
  #define ATOMIC_CLEAR(x)         x.clear()
 #else
  #define ATOMIC_VAR(n)           int n
  #define ATOMIC_SET(x, n)        x = n
  #define ATOMIC_READ(x)          x
  #define ATOMIC_RET_INC(x)       __sync_fetch_and_add(&x, 1)

  #define ATOMIC_FLAG(n)          long int n
  #define ATOMIC_TEST_AND_SET(x)  __sync_fetch_and_or(&x, 1)
  #define ATOMIC_CLEAR(x)         __sync_fetch_and_and(&x, ~1)
 #endif

#endif  // ! __KERNEL__



static LOCK_DECLARE(print_rwlock);
static int lock_initialised;


/**
 * Private struct tracept
 *
 * Contains data of one tracept created by make_tracept().
 *
 * time
 *   Timestamp of when the tracept is created
 * line
 *   Line number from where tracept is created
 * src
 *   Ptr to nul-terminated string that is available for the whole life time of
 *   tracept facility.
 *   See make_tracept() for detailed info.
 * msg
 *   Free text field for user input data.
 *
*/

struct tracept
{
	struct timeval time;
	
	int  line;
	char const *src;

	char msg[RT_MSG_MAX];
};


static struct tracept *tp_pool;
static struct tracept *tp_pool_copy;
static ATOMIC_VAR(next_tp);


/**
 * Private struct vspb_container (vsprintf buffer)
 *
 * This struct's purpose is to contain a helper buffer for vsnprintf to print in.
 * There are as many of these containers allocated in global vsprint_buffer as
 * the max_threads param given in runtrace_init().
 *
 * A number of threads might call tpprint() in parallel.
 *
 * To overcome this race condition, a process must inspect the in_use bit 0 atomically
 * when selecting a buffer for itself to use. The process must continue traversing
 * the buffers until it finds an unused buffer.
 *
 * The user can configure runtrace facility with max_threads parameter, which defines
 * how many vspb_containers are allocated. This max_threads should be set to value
 * corresponding how many parallel calls to tpprint() can occur. Proper configuration 
 * is essential for debugging sessions where multiple threads spam tpprint() 
 * continuously. Having fewer buffers than parallel calls results in some thread(s) 
 * being blocked in tpprint() waiting for an available buffer. Keep in mind that the 
 * number of parallel calls might be higher than the number of CPUs, because threads 
 * might get scheduled out while they're inside of tpprint().
 *
 * in_use
 *   Bit 0 denotes if the buffer is currently in use
 * next
 *   Buffers have a next ptr to the following buffer for performance reasons. The 
 *   last buffer's next ptr wraps around pointing to the first one.
 * buf
 *   Character buffer large enough for vsprintf to use
 *
*/

struct vspb_container
{
	ATOMIC_FLAG(in_use);
	struct vspb_container *next;
	char buf[RT_MSG_MAX];
};

static struct vspb_container *vsprint_buffer;

static int nr_cpus;
static int max_threads;

static int tp_pool_size;
static int tp_cnt_mask;
static int tp_pool_mem_size;

static int vspb_underrun_notice;


/**
 * (KERNEL) Character device globals
 *
*/

#ifdef __KERNEL__

static dev_t dev;
static struct cdev cdev;
static int major_number;


static int cdev_open(struct inode *inode, struct file *filp);
static int cdev_release(struct inode *inode, struct file *filp);
static ssize_t cdev_read(struct file *filp, char __user *buf, size_t count, loff_t *offp);
// static ssize_t cdev_write(struct file *filp, const char __user *, size_t, loff_t *);

static ssize_t cdev_write(struct file *filp, char const __user *buf, size_t count, loff_t *offp);

static struct file_operations cdev_fops = {
	.open    = cdev_open,
	.release = cdev_release,
	.read    = cdev_read,
	.write   = cdev_write,
	.llseek  = no_llseek
};

static ATOMIC_VAR(cdev_readers);
static DECLARE_WAIT_QUEUE_HEAD(read_queue);
static int cdev_blocking_read = 1;

#endif  // __KERNEL__


#ifdef __KERNEL__
 #define RT_WARNING(x...)  do { printk(KERN_WARNING __FILE__ ": " x); } while (0)
 // In kernel space, I'd not take such drastic measures as calling abort(), but
 // maybe try to be as noticeable as possible.
 #define RT_DISABLING_ERR(x...)  do { printk(KERN_ALERT __FILE__ ": " x); } while (0)
#else
 #define RT_WARNING(x...)  do { fprintf(stderr, __FILE__ ": " x); } while (0)
 #define RT_DISABLING_ERR(x...)  do { fprintf(stderr, __FILE__ ": " x); abort(); } while (0)
#endif




static void
print_vsbp_underrun_notice(void)
{
	RT_WARNING("************************************************************\n");
	RT_WARNING("Parallel calls exceeded the number of threads at some point.\n");
	RT_WARNING("You might want to configure higher nr_of_threads. (%d)\n", max_threads);
	RT_WARNING("************************************************************\n");
}

/**
 * Private function sigabrt()
 *
 * This is handler for SIGABRT signal.
 *
 * Return
 *   void
 *
*/

#ifndef __KERNEL__

static int flush_on_abort;
static int flush_with_flags;


static void
sigabort(int const sig)
{
	if (SIGABRT == sig  &&  flush_on_abort) {
		fprintf(stderr, 
			"**************************************\n"
			"SIGABRT called! Flooding runtrace pool\n"
			"**************************************\n");
		print_trace_stack(flush_with_flags);
	}

	if (vspb_underrun_notice) {
		print_vsbp_underrun_notice();
	}
}

#endif  // ! __KERNEL__


/**
 * Private function __print_trace_point()
 * Prints out one tracept.
 *
 * WARNING! MUST BE CALLED FROM LOCKED CONTEXT!
 *
 * flags
 *   Bitmask of flags telling how to print (see runtrace.h RT_PRINT_*)
 * dst 
 *   Destination buffer ptr where to print to. If NULL, output is directed to printk or stderr
 * tp 
 *   Ptr to struct tracept to print
 * prev_time 
 *   Ptr to previously printed tp's time. This is read for calculating time difference and 
 *   written for current tp's timestamp. Ptr may be NULL.
 *
 * Return
 *   Number of bytes when writing to buffer (dst != NULL),
 *   0 otherwise
 *   Cannot fail.
 *
*/

static int
__print_trace_point(int const flags, char *const dst, struct tracept const *const tp, u64 *const prev_time)
{
#ifdef __KERNEL__
 #define PRINT(p, dst, x, ...)  \
    if (!dst)  printk(KERN_INFO x, __VA_ARGS__); \
    else       p += sprintf(dst + p, x, __VA_ARGS__)
#else
 #define PRINT(p, dst, x, ...)  \
    if (!dst)  fprintf(stderr, x, __VA_ARGS__); \
    else       p += sprintf(dst + p, x, __VA_ARGS__)
#endif


	u64 time;
	unsigned int time_diff = 0;
	int printed = 0;

	int const print_time = flags & RT_PRINT_TIME;
	int const print_diff = flags & RT_PRINT_DIFF;
	int const print_line = flags & RT_PRINT_LINE;
	int const print_funk = flags & RT_PRINT_SRC;
	int const print_ms   = flags & RT_PRINT_TIME_MS;

#ifdef __KERNEL__
	if (!dst)  printk(KERN_ERR);
#endif

	// PRINT(PRINT_OUT "(%ld) ", tp - tp_pool_copy);

	if (print_ms) {
		time = tp->time.tv_sec * 1000 + tp->time.tv_usec / 1000;
	}
	else {
		time = tp->time.tv_sec * 1000000 + tp->time.tv_usec;
	}

	if (*prev_time) {
		time_diff = (unsigned int) (time - *prev_time);
	}


	if (print_ms) {
		if (print_time) {
			PRINT(printed, dst, "%6u.%03u ", (unsigned int) (time / 1000), (unsigned int) (time % 1000));
		}
		if (print_diff) {
			PRINT(printed, dst, "%+3d ", time_diff);
		}
	}
	else {
		if (print_time) {
			PRINT(printed, dst, "%6u.%06u ", (unsigned int) (time / 1000000), (unsigned int) (time % 1000000));
		}
		if (print_diff) {
			PRINT(printed, dst, "%+6d ", time_diff);
		}
	}

	if (print_funk) {
		PRINT(printed, dst, "%s ", tp->src);
	}
	if (print_line) {
		PRINT(printed, dst, "%5d ", tp->line);
	}

	PRINT(printed, dst, "%s\n", tp->msg);

	*prev_time = time;

	return printed;
}


/**
 * User API function print_trace_stack()
 * Prints out the whole tracept stack. Is multithread safe and can be called
 * from interrupt context.
 *
 * flags
 *   Bitmask of flags telling how to print (see runtrace.h RT_PRINT_*)
 * dst 
 *   Destination buffer ptr where to print to. If NULL, output is directed to printk or stderr
 * size
 *   Size of the buffer pointed by dst
 * priv
 *   Pointer to an integer of print_trace_stack's private data.
 *   If dst buffer's size was not enough to print whole tracept stack, the identifier of
 *   the next to be printed tracept is stored in priv's memory location. priv may be NULL. 
 *
 * Return
 *   Number of bytes written to buffer pointed by dst. 
 *   0 if dst == NULL or there is nothing to print.
 *   Cannot fail.
 *
*/

int
print_trace_stack(int const flags, char *const dst, int size, int *const priv)
{
	//
	// The tracept pool memory in whole is copied to another buffer
	// for the print function to use. This is because copying the buffer
	// is much faster than printing its contents.
	//
	// On a core i5 machine it takes about 110 times longer to print out the
	// contents than copy it for a userspace application. The timing was
	// tested with 2 writer threads (total 4 cores on the cpu) to have least
	// interruptions on the printing thread by scheduler as possible.
	//
	// 
	// On average, the time consumed per operation by:
	// - 1000 print operations was ~8800 usec
	// - 1000 memcpy operations was ~77 usec.
	//
	// The write lock must be held only when operating on common tp_pool buffer.
	//

	// static int const tp_pool_mem_size = (tp_cnt_mask+1) * sizeof(struct tracept);
	// static struct tracept *tp_pool_copy = tp_pool;

	int tp_num;
	int last_tp;
	int printed = 0;
	struct tracept const *tp;
	u64 prev_time = 0;
	int overrun = 0;

	// Worst-case guess for the size needed in buffer per one tracept record:
	// - 30 chars for absolute time + diff to previous time
	// - 30 chars for file name and line number
	int const needed_bufsize = 30 + 30 + RT_MSG_MAX;
	struct tracept const *const tp_end = tp_pool_copy + tp_cnt_mask;

// static u64 cnts;
// static u64 total;
// static u64 gtotal;
// struct timeval start, stop;


// GETTIMEOFDAY(&start);
	
	WRLOCK(print_rwlock);
	last_tp = ATOMIC_READ(next_tp);
	memcpy(tp_pool_copy, tp_pool, tp_pool_mem_size);
	WRUNLOCK(print_rwlock);

	
	if (size < needed_bufsize) {
		RT_DISABLING_ERR("Print buffer is too small\n");
		return -1;
	}

// GETTIMEOFDAY(&stop);

	// Adjust tp_num to the oldest record of ring buffer.
	if (priv) {
		if (last_tp == *priv) {
			return 0;
		}

		tp_num = *priv;
		
		// The next_tp uses int's whole range. We need to check if 0 is in middle of current
		// ring buffer. F.ex. with tp_num 100 and tp_pool_size 256, the pool's records go
		// from [-156..99].
		if ((int) (last_tp - tp_cnt_mask) < 0) {
			if (tp_num > last_tp  &&  tp_num < last_tp - tp_pool_size) {
				tp_num = last_tp - tp_pool_size;
				overrun = 1;
			}
		}
		else {
			if (tp_num < last_tp - tp_pool_size  ||  tp_num > last_tp) {
				tp_num = last_tp - tp_pool_size;
				overrun = 1;
			}
		}
	}
	else {
		tp_num = last_tp - tp_pool_size;
	}


	tp = tp_pool_copy + (tp_num & tp_cnt_mask);

	// Print ellipsis to indicate that there might be a cap to previous print's tracepoints
	if (overrun  &&  tp->src  &&  dst) {
		PRINT(printed, dst + printed, "...%c", '\n');
	}

	for (; tp_num != last_tp; ++tp_num) {
		if (tp->src) {
			// Overly protective sanity check
			if (dst  &&  printed > size - needed_bufsize) {
				break;
			}
			
			printed += __print_trace_point(flags, dst + printed, tp, &prev_time);
		}

		if (++tp > tp_end) {
			tp = tp_pool_copy;
		}
	}

	// UNLOCK(print_rwlock);
// GETTIMEOFDAY(&stop);
// ++cnts;
// if (stop.tv_sec != start.tv_sec)  stop.tv_usec += 1000000;

// 	total += stop.tv_usec - start.tv_usec;
// PRINT(PRINT_OUT "LCOKED TIME CONSUMED %6lu, TIMES %llu, AVERAGE %6llu\n", stop.tv_usec - start.tv_usec, cnts, total / cnts);
// if (cnts >= 1000) abort();
	

	if (priv) {
		*priv = tp_num;
	}

	return printed;
}



#ifdef __KERNEL__

/**
 * Kernel callback function cdev_open()
 *
 * Standard open function for file operations.
 * Initialises the f_pos to point beyond the oldest tracept.
 *
*/

static int 
cdev_open(struct inode *const inode, struct file *const filp)
{
#ifdef MODULE
	if (!try_module_get(THIS_MODULE)) {
		return -EACCES;
	}
#endif

	if (ATOMIC_RET_INC(cdev_readers) > 1) {
		ATOMIC_DEC(cdev_readers);
		return -EBUSY;
	}

	// Place print pointer to 1 record behind the oldest one
	filp->f_pos = (unsigned int) (ATOMIC_READ(next_tp) - tp_pool_size - 1);
	return nonseekable_open(inode, filp);
}


/**
 * Kernel callback function cdev_release()
 *
 * Decrement file usage count.
 *
*/

static int
cdev_release(struct inode *const inode, struct file *const filp)
{
	ATOMIC_DEC(cdev_readers);
#ifdef MODULE
	module_put(THIS_MODULE);
#endif
	return 0;
}


/**
 * Kernel callback function cdev_read()
 *
 * Standard read function for file operations.
 * Calls trace stack printing. File's offset data is used to keep track of printing
 * process.
 *
*/

static ssize_t 
cdev_read(struct file *const filp, char __user *const buf, size_t count, loff_t *const offp)
{
	int ret;

	do {
		while (*(unsigned int *) offp == ATOMIC_READ(next_tp)) {
			if (filp->f_flags & O_NONBLOCK) {
				return -EAGAIN;
			}
			
			// This is a convenience function for f.ex. cat. You can make cat non-blocking by typing
			// 'blocking 0' in the device file.
			if (!cdev_blocking_read) {
				return 0;
			}

			if (wait_event_interruptible(read_queue, *(unsigned int *) offp != ATOMIC_READ(next_tp))) {
				return -ERESTARTSYS;
			}
		}

		ret = print_trace_stack(RT_PRINT_DEFAULT, buf, count, (unsigned int *) offp);

	} while (0 == ret);

	return ret;
}


/**
 * Kernel callback function cdev_write()
 *
 * Standard read function for file operations.
 * For user to change between blocking and non-blocking read methods.
 *
*/

static ssize_t
cdev_write(struct file *const filp, char const __user *const buf, size_t const count, loff_t *const offp)
{
	unsigned char local_buf[20];
	size_t const len = count > sizeof(local_buf) ? sizeof(local_buf) : count;

	static unsigned char const b0[] = "blocking 0";
	static unsigned char const b1[] = "blocking 1";


	*local_buf = '\0';
	copy_from_user(local_buf, buf, len);

	if (!strncmp(local_buf, b0, strlen(b0))) {
		cdev_blocking_read = 0;
	}
	else if (!strncmp(local_buf, b1, strlen(b1))) {
		cdev_blocking_read = 1;
	}
	else {
		RT_WARNING("Unknown cmd in %s: %s\n", __FUNCTION__, local_buf);
		return -EINVAL;
	}

	// Make the impression we just read everything
	return count;
}

#endif  // __KERNEL__


/**
 * Private function environment_check()
 *
 * Reads user set environment variables for runtrace facility configuration data.
 * Reconfigure runtrace if such variables is found.
 *
 * Return
 *   void
 *
*/

static void
environment_check(int *const pool_size)
{
#ifdef __KERNEL__
	(void) pool_size;
#else
	char const *const pool_size_env = getenv("RT_POOL_SIZE");
	int i = 0;

	if (pool_size_env) {
		i = atoi(pool_size_env);
		if (i > 0) {
			*pool_size = i;
		}
	}
#endif
}


/**
 * (USERSPACE) User API function runtrace_flush_on_abort()
 *
 * enable
 *   0 to disable the feature, otherwise enable
 *
 * If SIGABRT is called on running application and flush on abort feature
 * is enabled the runtrace stack is printed to stderr.
 *
 * Return
 *   void
 *
*/

#ifndef __KERNEL__

void
runtrace_flush_on_abort(int const enable, int const print_flags)
{
	flush_on_abort = enable;

	if (enable) {
		flush_with_flags = print_flags;
	}
}

#endif  // __KERNEL__


/**
 * User API function runtrace_reconfigure()
 *
 * nr_of_threads
 *   The number of parallel threads runtrace facility should be prepared to handle.
 *   See more about this in comments of struct vspb_container.
 *
 * pool_size
 *   New desired size for tracept pool.
 *
 * If pool_size passes sanity checks, the lock is obtained (if needed) and the
 * tracept pool is reinitialised. This process clears the pool of its previous
 * contents.
 *
 * Return
 *   -1 if pool_size sanity check failed or memory allocation fails.
 *   0  on success
 *
*/

int
runtrace_reconfigure(int nr_of_threads, int const pool_size)
{
	int i;


	if (nr_of_threads < 1) {
		RT_DISABLING_ERR("Invalid nr of threads param in %s (%d)\n", __FUNCTION__, nr_of_threads);
		RT_WARNING("Assuming 1 thread!");
		nr_of_threads = 1;
	}
	if (pool_size < 4) {
		RT_DISABLING_ERR("%s called with invalid pool_size %d (really?)\n", __FUNCTION__, pool_size);
		return -1;
	}
	if (0 != (pool_size & (pool_size-1))) {
		RT_DISABLING_ERR("%s called with invalid pool_size %d (not power of 2)\n", __FUNCTION__, pool_size);
		return -1;
	}

	if (lock_initialised) {
		WRLOCK(print_rwlock);
	}
	
	// nr_cpus is set in runtrace_init() because it's constant in this contexts
	max_threads = nr_of_threads;
	tp_pool_size = pool_size;
	tp_cnt_mask = tp_pool_size - 1;  // Forms a mask from 2^size
	tp_pool_mem_size = tp_pool_size * sizeof(struct tracept);

	
	if (tp_pool)
		FREE(tp_pool);
	if (tp_pool_copy)
		FREE(tp_pool_copy);
	if (vsprint_buffer)
		FREE(vsprint_buffer);

	tp_pool = (struct tracept *) MALLOC(tp_pool_mem_size);
	tp_pool_copy = (struct tracept *) MALLOC(tp_pool_mem_size);
	vsprint_buffer = (struct vspb_container *) MALLOC(max_threads * sizeof(struct vspb_container));


	if (!tp_pool  ||  !tp_pool_copy  ||  !vsprint_buffer) {
		if (tp_pool) {
			FREE(tp_pool); tp_pool = NULL;
		}
		if (tp_pool_copy) {
			FREE(tp_pool_copy); tp_pool_copy = NULL;
		}
		if (vsprint_buffer) {
			FREE(vsprint_buffer); vsprint_buffer = NULL;
		}

		RT_DISABLING_ERR("Cannot allocate %d bytes for tracept pool and its copy buffer", tp_pool_mem_size);
		return -1;
	}


	memset(tp_pool, 0, tp_pool_mem_size);
	for (i=0; i<max_threads; ++i) {
		ATOMIC_CLEAR(vsprint_buffer[i].in_use);
		vsprint_buffer[i].next = &vsprint_buffer[i+1];
	}
	vsprint_buffer[i-1].next = &vsprint_buffer[0];
	memset(&vsprint_buffer[2], 0xed, 4);

	if (lock_initialised) {
		WRUNLOCK(print_rwlock);
	}
	
	return 0;
}


/**
 * User API function runtrace_init()
 *
 * nr_of_threads
 *   The number of parallel threads runtrace facility should be prepared to handle.
 *   See more about this in comments of struct vspb_container.
 *
 * Initialises runtrace facility. This must be called before other user API 
 * functions, except runtrace_reconfigure, are called.
 * Result of calling this function multiple times is undetermined.
 *
 * Return
 *   -1 if runtrace_reconfigure call fails or (KERNEL) chardev cannot be opened.
 *   0  on success
 *
*/

int
runtrace_init(int const nr_of_threads)
{
	int pool_size = RT_POOL_SIZE_DFLT;


	ATOMIC_SET(next_tp, 0);
	nr_cpus = NR_CPUS;

	environment_check(&pool_size);

	if (runtrace_reconfigure(nr_of_threads, pool_size) < 0 ) {
		return -1;
	}

	LOCK_INIT(print_rwlock);
	lock_initialised = 1;


#ifdef __KERNEL__
	if (alloc_chrdev_region(&dev, 0, 1, "runtrace")) {
		RT_DISABLING_ERR("Can't allocate chrdev region\n");
		return -1;
	}
	major_number = MAJOR(dev);
	cdev_init(&cdev, &cdev_fops);
#ifdef MODULE
	cdev.owner = THIS_MODULE;
#endif

	if (cdev_add(&cdev, MKDEV(major_number, 0), 1)) {
		RT_DISABLING_ERR("Can't add chrdev %d:0\n", major_number);
		return -1;
	}

	printk(KERN_INFO "rt_mod_test: Created cdev %d:0\nUse mknod /dev/rt_mod_test c %d 0\n", major_number, major_number);

#else  // ! __KERNEL__

	signal(SIGABRT, sigabort);

#endif

	return 0;
}


/**
 * User API function runtrace_exit()
 *
 * Releases all resources used by runtrace facility. Call this on your
 * application exit or kernel module unloading.
 *
 * Return
 *   void
 *
*/

void
runtrace_exit(void)
{
#ifdef __KERNEL__
	cdev_del(&cdev);
	unregister_chrdev_region(dev, 1);
#endif

	if (vspb_underrun_notice) {
		print_vsbp_underrun_notice();
	}

	if (tp_pool) {
		FREE(tp_pool);
		tp_pool = NULL;
	}
	if (tp_pool_copy) {
		FREE(tp_pool_copy);
		tp_pool_copy = NULL;
	}
	if (vsprint_buffer) {
		FREE(vsprint_buffer);
		vsprint_buffer = NULL;
	}

	LOCK_DESTROY(print_rwlock);
	lock_initialised = 0;
}


/**
 * User API function make_tracept()
 * Creates a tracept. This is multithread safe and can be called from interrupt context.
 *
 * Several instances of make_tracept can run parallel in read lock protected section.
 * The only critical data, next_tp, is read and incremented atomically.
 *
 * line
 *   Line number of the calling context.
 * src
 *   File or function name of the calling context. MUST BE of 'char const' type and
 *   MUST BE available for the whole life time of tracept facility. F.ex. precompiler
 *   macros __FILE__ and __FUNCTION__ will do nicely.
 * msg
 *   Ptr to a buffer ending on null-terminated string containing RT_MSG_MAX characters
 *   at most. Can be NULL.
*/

void
make_tracept(int const line, char const *const src, char const *const msg)
{
	struct tracept *tp;


	if (!tp_pool) {
		RT_DISABLING_ERR("You did not call init_tracepts()\n");
		// When in kernel, try to initialise runtrace facility().
		// Userspace app calls abort() in DISABLING_ERR
		runtrace_init(1);
	}
	

	RDLOCK(print_rwlock);

	// Although multiple threads run parallel in read lock protected section, the next_tp
	// incrementation must be guarded against printing function. 
	// Printing function calls invokes write lock to prevent other threads from changing 
	// contents of the tracept buffer and the next_tp variable.
	tp = tp_pool + (tp_cnt_mask & ATOMIC_RET_INC(next_tp));

	GETTIMEOFDAY(&tp->time);
	tp->line = line;
	tp->src = src;
	
	if (!msg)
		*tp->msg = '\0';
	else 
		strncpy(tp->msg, msg, RT_MSG_MAX);

	RDUNLOCK(print_rwlock);

	wake_up_interruptible(&read_queue);
}


/**
 * User API function print_tracept()
 *
 * line, src
 *   See make_tracept()
 * fmt
 *   Printf-style format string
 *
 * This is a convenience printing function that calls make_tracept() with
 * format string printed out.
 *
 * For details about vsnprintf buffer rotation, see struct vspb_container.
 *
*/

int
tpprintf(int line, char const *src, char const *fmt, ...)
{
	struct vspb_container *container = vsprint_buffer;
	int printed;
	va_list args;


	if (!vsprint_buffer) {
		RT_DISABLING_ERR("You did not call init_tracepts()\n");
		// When in kernel, try to initialise runtrace facility().
		// Userspace app calls abort() in DISABLING_ERR
		runtrace_init(1);
	}


	if (!fmt) {
		make_tracept(line, src, NULL);	
		return 0;
	}


	while (ATOMIC_TEST_AND_SET(container->in_use)) {
		container = container->next;

		// If we go around full set of vsprint_buffers all the buffers are in use.
		if (container == vsprint_buffer) {
#ifndef __KERNEL__
			// If the number of buffers (equal to max_threads) is greater than 
			// the number of CPUs, it means there's thread(s) scheduled out while 
			// holding a buffer. 
			// We can yield the thread to let other threads run to free the buffers.
			if (max_threads > nr_cpus) {
				sched_yield();
			}
#endif

			vspb_underrun_notice = 1;
		}
	}

	va_start (args, fmt);
	printed = vsnprintf(container->buf, RT_MSG_MAX, fmt, args);
	
	make_tracept(line, src, container->buf);
	ATOMIC_CLEAR(container->in_use);

	va_end(args);

	return printed;
}

