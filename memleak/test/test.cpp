
#include <stdio.h>
#include <stdlib.h>
#include <list>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "../memleak.h"


// Pointer to integer cast macro
#ifdef __x86_64__
# define PTR_TO_INT_CAST  long long
#else
# define PTR_TO_INT_CAST  long
#endif


static bool running = true;

// #undef malloc

static void *
stress_thread(void *ThreadNum)
{
	char const *const list_of_files[] = {
		"Abba.cpp", "Science.cpp", "Jackson.cpp", "System.cpp", "acdc.cpp", "Kurt.cpp", "Doors.cpp",
		"IronMaiden.cpp", "Slayer.cpp", "Meshuggah.cpp", "Kvelertak.cpp", "AmonAmarth.cpp"};
	const int files = sizeof(list_of_files) / sizeof(list_of_files[0]);
	char const *file;
	int line;
	int size;
	int allocations = 0;
	int i;
	struct timeval tv;
	const int DesiredLevel = 1000;
	const int Deviation = 100;
	const int MaxSize = 2000;
	const int MinSize = 4;
	char *ptrs[DesiredLevel + Deviation];
	char **pptr;
	int  FirstAvail = 0;
	int  do_or_dare;
	
	// Initialise random seed
	gettimeofday(&tv, NULL);
	srand(tv.tv_sec);
	
	fprintf(stderr, "Thread %d starting\n", (int) (PTR_TO_INT_CAST) ThreadNum);
	
	i = 1;
	for (pptr = ptrs; pptr < ptrs + (DesiredLevel + Deviation); ++pptr) {
		*pptr = (char *) (long long) i;
		++i;
	}
	
	// for (allocations = 0; allocations < DesiredLevel; ++allocations) {
		// size = rand() % (MaxSize - MinSize) + MinSize;
		
		// pptr = ptrs + FirstAvail;
		// FirstAvail = (int) (*(ptrs + FirstAvail));
		// *pptr = (char *) malloc(size);
	// }
	
	
	while (running)
	{
		do_or_dare = rand() % (Deviation * 2) + 1;
		
		if (allocations - (DesiredLevel - Deviation) < do_or_dare)
		{
			// Allocate
			file = list_of_files[rand() % files];
			line = rand() % 3 + 10;
			size = rand() % (MaxSize - MinSize) + MinSize;
			
			pptr = ptrs + FirstAvail;
			FirstAvail = (PTR_TO_INT_CAST) (*(ptrs + FirstAvail));
			*pptr = (char *) ss_malloc(size, file, line);
			
			++allocations;
		}
		else
		{
			do {
				pptr = ptrs + rand() % (DesiredLevel + Deviation);
			
			} while ((PTR_TO_INT_CAST) (*pptr) <= DesiredLevel + Deviation);
			
			free(*pptr);
			*pptr = (char *) (long long) FirstAvail;
			FirstAvail = pptr - ptrs;

			--allocations;
		}
		
		
		
		++i;
		if (i % 1000000 == 0) {
			fprintf(stderr, "allocations = %d\n", allocations);
			memleak_report(MEML_TIGHT | MEML_SUPPRESS_ZEROS);
		}
	}
	
	exit(0);
}


static void
stress_test(void)
{
	int Threads = 3;
	pthread_t Thread[Threads-1];

	
	while (Threads > 1) {
		if (pthread_create(&Thread[Threads-1], NULL, stress_thread, (void *) (long long) Threads)) {
			fprintf(stderr, "Failed to pthread: %s\n", strerror(errno));
			exit(1);
		}
		
		--Threads;
	}
	
	stress_thread((void *) (long long) Threads);
}


struct Bar
{
	int a;
	float c;
};


static void 
signal_handler(int sig)
{
	switch (sig) {
	// Interrupt from keyboard
	case SIGINT:
		running = false;
	break;

	default:
		fprintf(stderr, "Caught signal %d with no implemented handler\n", sig);
	break;
	}
}


int main(int const argc, char *const argv[])
{
	//
	// REMEMBER TO ENABLE TRACKING!
	// 
	ss_memleak_tracking = 1;

	bool runBasicTest = false;
	bool runStressTest = false;

	if (argc < 2) {
		// This is default when no arguments are given
		runBasicTest = true;
	}
	else {
		for (int i=1; i<argc; ++i) {
			if (!strcmp(argv[i], "stress")) {
				runStressTest = true;
			}
			else if (!strcmp(argv[i], "basic")) {
				runBasicTest = true;
			}
		}
	}


	if (runBasicTest) {
		char *p  = (char *) malloc(6);
		char *n  = new char;  // !! This will leak !!
		Bar *bar = new Bar;
		Bar *bar2 = new Bar[5];
		char *m  = new char[100];
		
		// You can also call ss_malloc directly with bogus file name and line number
		// !! This will leak !!
		ss_malloc(55, "humbug", 22);
		
		// If memleak.h is included after include <list>, these
		// actions won't be recorded.
		std::list<int> list;
		for (int i=0; i<10; ++i) list.push_back(10);
		
		// This will cause four new leaked blocks
		for (int i=0; i<5; ++i) n = new char[128];  // !! 4 of these will leak !!
		
		delete bar;
		memleak_report(/*MEML_TIGHT | MEML_SUPPRESS_ZEROS*/);
		
		delete [] n;
		free(p);
		delete [] m;
		delete bar2;
		
		// No need to print final results here if SS_ENABLE_MEMTRACE_EXIT is defined
		// memleak_report();
	}
	
	if (runStressTest) {
		signal(SIGINT, signal_handler);
		stress_test();
	}
	
	return 0;
}

