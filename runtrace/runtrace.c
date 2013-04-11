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
 #include <linux/kernel.h>
 #include <linux/slab.h>
 #include <linux/time.h>
 #include <linux/sched.h>
 #include <linux/fs.h>
 #include <linux/cdev.h>
 #include <asm/atomic.h>
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

 #define MALLOC(x)           kmalloc(x, GFP_KERNEL)
 #define FREE(x)             kfree(x)
 #define GETTIMEOFDAY(x)     do_gettimeofday(x)
 // #define NR_CPUS  This is defined in kernel headers
 
 #define LOCK_DECLARE(x)     DEFINE_RWLOCK(x)
 #define LOCK_INIT(x)        do {} while(0);
 #define LOCK_DESTROY(x)     do {} while(0);
 #define RDLOCK(x)           read_lock(&x)
 #define WRLOCK(x)           write_lock(&x)
 #define RDUNLOCK(x)         read_unlock(&x)
 #define WRUNLOCK(x)         write_unlock(&x)
 
 #define ATOMIC_VAR(n)       atomic_t n
 #define ATOMIC_SET(x, n)    atomic_set(&x, n)
 #define ATOMIC_READ(x)      atomic_read(&x)
 #define ATOMIC_INC_RET(x)   (atomic_inc_return(&x))

 #define ATOMIC_FLAG(n)      long int n
 #define ATOMIC_TEST_AND_SET(x)  test_and_set_bit(0, &x)
 #define ATOMIC_CLEAR(x)     clear_bit(0, &x)

#else  // ! __KERNEL__

 #define MALLOC(x)           malloc(x)
 #define FREE(x)             free(x)
 #define GETTIMEOFDAY(x)     gettimeofday(x, NULL)
 #define NR_CPUS             sysconf(_SC_NPROCESSORS_ONLN)
 #define u64                 unsigned long long

 #define LOCK_DECLARE(x)     pthread_rwlock_t x;
 #define LOCK_INIT(x)        pthread_rwlockattr_t x##_attr; \
                             pthread_rwlockattr_setkind_np(&x##_attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP); \
                             pthread_rwlock_init(&x, &x##_attr);
 #define LOCK_DESTROY(x)     pthread_rwlock_destroy(&x)
 #define RDLOCK(x)           pthread_rwlock_rdlock(&x)
 #define WRLOCK(x)           pthread_rwlock_wrlock(&x)
 #define RDUNLOCK(x)         pthread_rwlock_unlock(&x)
 #define WRUNLOCK(x)         pthread_rwlock_unlock(&x)

 #if defined __cplusplus  &&  defined __GXX_EXPERIMENTAL_CXX0X__
  #define ATOMIC_VAR(n)       std::atomic<unsigned int> n
  #define ATOMIC_SET(x, n)    x = n
  #define ATOMIC_READ(x)      x
  #define ATOMIC_INC_RET(x)   (x.fetch_add(1) + 1)

  #define ATOMIC_FLAG(n)      std::atomic_flag n
  #define ATOMIC_TEST_AND_SET(x)  x.test_and_set()
  #define ATOMIC_CLEAR(x)     x.clear()
 #else
  #define ATOMIC_VAR(n)       int n
  #define ATOMIC_SET(x, n)    x = n
  #define ATOMIC_READ(x)      x
  #define ATOMIC_INC_RET(x)   (__sync_fetch_and_add(&x, 1) + 1)

  #define ATOMIC_FLAG(n)      long int n
  #define ATOMIC_TEST_AND_SET(x)  __atomic_test_and_set(&x, 0)
  #define ATOMIC_CLEAR(x)     __atomic_clear(&x, 0)
 #endif

#endif  // ! __KERNEL__



static LOCK_DECLARE(print_rwlock);
static int lock_initialised;


/**
 * Private struct tracepoint
 *
 * Contains data of one tracepoint created by make_tracepoint().
 *
 * time
 *   Timestamp of when the tracepoint is created
 * line
 *   Line number from where tracepoint is created
 * src
 *   Ptr to nul-terminated string that is available for the whole life time of
 *   tracepoint facility.
 *   See make_tracepoint() for detailed info.
 * msg
 *   Free text field for user input data.
 *
*/

struct tracepoint
{
	struct timeval time;
	
	int  line;
	char const *src;

	char msg[RT_MSG_MAX];
};


static struct tracepoint *tp_pool;
static struct tracepoint *tp_pool_copy;
static ATOMIC_VAR(curr_tp);


/**
 * Private struct vspb_container (vsprintf buffer)
 *
 * This struct's purpose is to contain a helper buffer for vsnprintf to print in.
 * There are as many of these containers allocated in global vsprint_buffer as
 * there are CPUs in the machine.
 * To overcome race condition, a process must inspect the in_use bit 0 atomically
 * when selecting a buffer for itself to use. The process must continue traversing
 * the buffers until it finds an unused buffer.
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

static int tp_pool_size;
static int tp_cnt_mask;
static int tp_pool_mem_size;


/**
 * (KERNEL) Character device globals
 *
*/

#ifdef __KERNEL__

static dev_t dev;
static struct cdev cdev;
static int major_number;


static int cdev_open(struct inode *inode, struct file *filp);
static ssize_t cdev_read(struct file *filp, char __user *buf, size_t count, loff_t *offp);

static struct file_operations cdev_fops = {
	// .owner   = THIS_MODULE,
	.open    = cdev_open,
	.read    = cdev_read,
	.llseek  = no_llseek
};

#endif  // __KERNEL__


#ifdef __KERNEL__
 #define WARNING(x...)  printk(KERN_WARNING __FILE__ ": " x)
 // In kernel space, I'd not take such drastic measures as calling abort(), but
 // maybe try to be as noticeable as possible.
 #define DISABLING_ERR(x...)  printk(KERN_ALERT __FILE__ ": " x)
#else
 #define WARNING(x...)  fprintf(stderr, __FILE__ ": " x)
 #define DISABLING_ERR(x...)  fprintf(stderr, __FILE__ ": " x); abort()
#endif




/**
 * Private function sigabrt()
 *
 * This is handler for SIGABRT signal. If
 *
 * Return
 *   void
 *
*/

#ifndef __KERNEL__

static int flush_on_abort;
static int flush_with_flags;


void sigabort(int const sig)
{
	if (SIGABRT == sig  &&  flush_on_abort) {
		fprintf(stderr, 
			"**************************************\n"
			"SIGABRT called! Flooding runtrace pool\n"
			"**************************************\n");
		print_trace_stack(flush_with_flags);
	}
}

#endif  // ! __KERNEL__


/**
 * Private function __print_trace_point()
 * Prints out one tracepoint.
 *
 * WARNING! MUST BE CALLED FROM LOCKED CONTEXT!
 *
 * flags
 *   Bitmast of flags telling how to print (see runtrace.h RT_PRINT_*)
 * dst 
 *   Destination buffer ptr where to print to. If NULL, output is directed to printk or stderr
 * tp 
 *   Ptr to struct tracepoint to print
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
__print_trace_point(int const flags, char *const dst, struct tracepoint const *const tp, u64 *const prev_time)
{
#ifdef __KERNEL__
 #define PRINT(p, dst, x, ...)  \
    if (!dst)  printk(x, __VA_ARGS__); \
    else       p += sprintf(dst + p, x, __VA_ARGS__)
#else
 #define PRINT(p, dst, x, ...)  \
    if (!dst)  fprintf(stderr, x, __VA_ARGS__); \
    else       p += sprintf(dst + p, x, __VA_ARGS__)
#endif


#ifdef __GXX_EXPERIMENTAL_CXX0X__
// #error Restore u64 time;
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
 * Prints out the whole tracepoint stack. Is multithread safe and can be called
 * from interrupt context.
 *
 * flags
 *   Bitmast of flags telling how to print (see runtrace.h RT_PRINT_*)
 * dst 
 *   Destination buffer ptr where to print to. If NULL, output is directed to printk or stderr
 * size
 *   Size of the buffer pointed by dst
 * priv
 *   Pointer to an integer of print_trace_stack's private data.
 *   If dst buffer's size was not enough to print whole tracepoint stack, the identifier of
 *   the next to be printed tracepoint is stored in priv's memory location. priv may be NULL. 
 *
 * Return
 *   Number of bytes written to buffer pointed by dst. 
 *   0 if dst == NULL or there is nothing to print.
 *   Cannot fail.
 *
*/

int
print_trace_stack(int const flags, char *const dst, int const size, int *const priv)
{
	//
	// The tracepoint pool memory in whole is copied to another buffer
	// for the print function to use. This is because copying the buffer
	// is much faster than printing its contents.
	//
	// On a core i5 machine it takes about x times longer to print out the
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

	// static int const tp_pool_mem_size = (tp_cnt_mask+1) * sizeof(struct tracepoint);
	// static struct tracepoint *tp_pool_copy = tp_pool;

	int tp_num;
	int last_tp;
	int printed = 0;
	int last_printed = 0;
	struct tracepoint const *tp;
	u64 prev_time = 0;

// static u64 cnts;
// static u64 total;
// static u64 gtotal;
// struct timeval start, stop;


// GETTIMEOFDAY(&start);
	
// #ifdef __KERNEL__
// printk(KERN_ERR "%s: %x, %p, %d, %p\n", __FUNCTION__, flags, dst, size, priv);
// #endif
	WRLOCK(print_rwlock);
	last_tp = ATOMIC_READ(curr_tp);
	memcpy(tp_pool_copy, tp_pool, tp_pool_mem_size);
	WRUNLOCK(print_rwlock);

	
	if (priv  &&  *priv == last_tp + 1) {
		return 0;
	}


// GETTIMEOFDAY(&stop);

	if (priv) {
		tp_num = *priv;
		if (tp_num < last_tp - tp_cnt_mask  ||  tp_num > last_tp) {
			tp_num = last_tp - tp_cnt_mask;
		}
	}
	else {
		tp_num = last_tp - tp_cnt_mask;
	}

	for (; tp_num <= last_tp; ++tp_num) {
		tp = tp_pool_copy + (tp_num & tp_cnt_mask);

		if (tp->src) {
			// Worst-case guess for the size needed in buffer per one tracepoint record:
			// - 30 chars for absolute time + diff to previous time
			// - 30 chars for file name and line number
			if (dst  &&  printed > size - (30 + 30 + RT_MSG_MAX)) {
				if (0 == printed) {
					DISABLING_ERR("Print buffer is too small\n");
				}
				break;
			}
			
			last_printed = __print_trace_point(flags, dst + printed, tp, &prev_time);
			printed += last_printed;
		}
		// else printk("s(%d) ", tp_num);
	}

	// UNLOCK(print_rwlock);
// GETTIMEOFDAY(&stop);
// ++cnts;
// if (stop.tv_sec != start.tv_sec)  stop.tv_usec += 1000000;

// 	total += stop.tv_usec - start.tv_usec;
// PRINT(PRINT_OUT "LCOKED TIME CONSUMED %6lu, TIMES %llu, AVERAGE %6llu\n", stop.tv_usec - start.tv_usec, cnts, total / cnts);
// if (cnts >= 1000) abort();
	

	if (priv) {
		// If 0 bytes was printed, there is possibility that the buffer did not suffice for
		// printing a single tracepoint.
		if (0 == printed)
			*priv = last_tp + 1;
		else 
			*priv = tp_num;
	}

	return printed;
}



#ifdef __KERNEL__

/**
 * Kernel callback function cdev_open()
 *
 * Standard open function for file operations.
 * Initialises the f_pos to point to the oldest tracepoint.
 *
*/

static int 
cdev_open(struct inode *const inode, struct file *const filp)
{
	filp->f_pos = (unsigned int) (ATOMIC_READ(curr_tp) - tp_cnt_mask);
	return nonseekable_open(inode, filp);
}


/**
 * Kernel callback function cdev_raed()
 *
 * Standard read function for file operations.
 * Calls trace stack printing. File's offset data is used to keep track of printing
 * process.
 *
*/

static ssize_t 
cdev_read(struct file *const filp, char __user *const buf, size_t count, loff_t *const offp)
{
	int const ret = print_trace_stack(RT_PRINT_DEFAULT, buf, count, (unsigned int *) offp);
	return ret;
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
environment_check(void)
{
#ifndef __KERNEL__
	char const *const pool_size_env = getenv("RT_POOL_SIZE");
	int i = 0;

	if (pool_size_env) {
		i = atoi(pool_size_env);
		if (i > 0) {
			runtrace_reconfigure(i);
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
 * pool_size
 *   New desired size for tracepoint pool.
 *
 * If pool_size passes sanity checks, the lock is obtained (if needed) and the
 * tracepoint pool is reinitialised. This process clears the pool of its previous
 * contents.
 *
 * Return
 *   -1 if pool_size sanity check failed or memory allocation fails.
 *   0  on success
 *
*/

int
runtrace_reconfigure(int const pool_size)
{
	int nr_cpus = NR_CPUS;
	int i;


	if (nr_cpus < 1) {
		WARNING("Could not detect number of CPUs. Assuming 4 for safety!");
		nr_cpus = 4;
	}
	if (pool_size < 4) {
		DISABLING_ERR("%s called with invalid pool_size %d (really?)\n", __FUNCTION__, pool_size);
		return -1;
	}
	if (0 != (pool_size & (pool_size-1))) {
		DISABLING_ERR("%s called with invalid pool_size %d (not power of 2)\n", __FUNCTION__, pool_size);
		return -1;
	}

	if (lock_initialised) {
		WRLOCK(print_rwlock);
	}
	
	tp_pool_size = pool_size;
	tp_cnt_mask = tp_pool_size - 1;  // Forms a mask from 2^size
	tp_pool_mem_size = tp_pool_size * sizeof(struct tracepoint);

	
	if (tp_pool)
		FREE(tp_pool);
	if (tp_pool_copy)
		FREE(tp_pool_copy);
	if (vsprint_buffer)
		FREE(vsprint_buffer);

	tp_pool = (struct tracepoint *) MALLOC(tp_pool_mem_size);
	tp_pool_copy = (struct tracepoint *) MALLOC(tp_pool_mem_size);
	vsprint_buffer = (struct vspb_container *) MALLOC(nr_cpus * sizeof(struct vspb_container));


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

		DISABLING_ERR("Cannot allocate %d bytes for tracepoint pool and its copy buffer", tp_pool_mem_size);
		return -1;
	}


	memset(tp_pool, 0, tp_pool_mem_size);
	for (i=0; i<nr_cpus; ++i) {
		// vsprint_buffer[i].in_use = 0;
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
runtrace_init()
{
	ATOMIC_SET(curr_tp, -1);

	environment_check();

	if (!tp_pool) {
		if (runtrace_reconfigure(RT_POOL_SIZE_DFLT) < 0 ) {
			return -1;
		}
	}

	LOCK_INIT(print_rwlock);
	lock_initialised = 1;


#ifdef __KERNEL__
	if (alloc_chrdev_region(&dev, 0, 1, "runtrace")) {
		DISABLING_ERR("Can't allocate chrdev region\n");
		return -1;
	}
	major_number = MAJOR(dev);
	cdev_init(&cdev, &cdev_fops);
	// cdev.owner = THIS_MODULE;
	
	if (cdev_add(&cdev, MKDEV(major_number, 0), 1)) {
		DISABLING_ERR("Can't add chrdev %d:0\n", major_number);
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
 * User API function make_tracepoint()
 * Creates a tracepoint. This is multithread safe and can be called from interrupt context.
 *
 * Several instances of make_tracepoint can run parallel in read lock protected section.
 * The only critical data, curr_tp, is read and incremented atomically.
 *
 * line
 *   Line number of the calling context.
 * src
 *   File or function name of the calling context. MUST BE of 'char const' type and
 *   MUST BE available for the whole life time of tracepoint facility. F.ex. precompiler
 *   macros __FILE__ and __FUNCTION__ will do nicely.
 * msg
 *   Ptr to a buffer ending on null-terminated string containing RT_MSG_MAX characters
 *   at most. Can be NULL.
*/

void
make_tracepoint(int const line, char const *const src, char const *const msg)
{
	struct tracepoint *tp;


	if (!tp_pool) {
		DISABLING_ERR("You did not call init_tracepoints()\n");
		// When in kernel, try to initialise runtrace facility().
		// Userspace app calls abort() in DISABLING_ERR
		runtrace_init();
	}
	

	RDLOCK(print_rwlock);

	// Although multiple threads run parallel in read lock protected section, the curr_tp
	// incrementation must be guarded against printing function. 
	// Printing function calls invokes write lock to prevent other threads from changing 
	// contents of the tracepoint buffer and the curr_tp variable.
	tp = tp_pool + (tp_cnt_mask & ATOMIC_INC_RET(curr_tp));

	GETTIMEOFDAY(&tp->time);
	tp->line = line;
	tp->src = src;
	
	if (!msg)
		*tp->msg = '\0';
	else 
		strncpy(tp->msg, msg, RT_MSG_MAX);

	RDUNLOCK(print_rwlock);
}


/**
 * User API function print_tracepoint()
 *
 * line, src
 *   See make_tracepoint()
 * fmt
 *   Printf-style format string
 *
 * This is a convenience printing function that calls make_tracepoint() with
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
		DISABLING_ERR("You did not call init_tracepoints()\n");
		// When in kernel, try to initialise runtrace facility().
		// Userspace app calls abort() in DISABLING_ERR
		runtrace_init();
	}


	if (!fmt) {
		make_tracepoint(line, src, NULL);	
		return 0;
	}


	while (ATOMIC_TEST_AND_SET(container->in_use)) {
		container = container->next;
	}

	va_start (args, fmt);
	printed = vsnprintf(container->buf, RT_MSG_MAX, fmt, args);
	
	make_tracepoint(line, src, container->buf);
	ATOMIC_CLEAR(container->in_use);

	va_end(args);

	return printed;
}

