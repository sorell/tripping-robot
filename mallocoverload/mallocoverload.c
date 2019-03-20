#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <stdio.h>

extern "C" {

void *malloc(size_t const size)
{
	typedef void *(*malloc_t)(size_t);
	static malloc_t libc_malloc = NULL;
	if (!libc_malloc) {
		libc_malloc = (malloc_t) dlsym(RTLD_NEXT, "malloc");
		if (!libc_malloc) {
			fprintf(stderr, "Can't dlsym malloc\n");
			return NULL;
		}
	}
	fprintf(stderr, "malloc(%lu)\n", size);  // Plain printf causes segfault
	return libc_malloc(size);
}


void free(void *const p)
{
	typedef void (*free_t)(void*);
	static free_t libc_free = NULL;
	if (!libc_free) {
		libc_free = (free_t) dlsym(RTLD_NEXT, "free");
		if (!libc_free) {
			fprintf(stderr, "Can't dlsym malloc\n");
			return;
		}
	}
	fprintf(stderr, "free(%p)\n", p);
	libc_free(p);
}

}  // extern "C"
