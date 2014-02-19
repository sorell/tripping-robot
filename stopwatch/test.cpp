#include <stdio.h>
#include <stdlib.h>
#include "stopwatch.h"


static void
wait_for(unsigned int const cycles)
{
	unsigned int i;
	unsigned int j;

	for (i=0; i<cycles; ++i) {
		for (j = 100000; j > 0; --j) asm("nop");
	}
}

int 
main(void)
{
	unsigned int cycles = 1;
	unsigned int run_for = 2;
	unsigned int i;

	
	STOPWATCH_INIT(a);
	STOPWATCH_START(a);

	/**
	 * Calibrating the number of cycles so that the whole wait_for() process takes
	 * more than 400ms.
	 * This is not, and is not supposed to be, scientifically accurate. F.ex. possible
	 * application scheduling may mess this up.
	 *
	*/
	do {
		cycles *= 10;
		wait_for(cycles);

		STOPWATCH_STOP(a);
		i = STOPWATCH_ELAPSED_US(a);

	} while (i < 400000);

	printf("Cycles calibrated to %u\nRunning for %d+ seconds\n\n", cycles, run_for);


	/**
	 * Do wait_for() loops until run_for seconds has passed.
	 * Then print out the elapsed time and do some simple comparison.
	 *
	*/
	STOPWATCH_START(a);

	do {
		wait_for(cycles);
		
		STOPWATCH_STOP(a);
		i = STOPWATCH_ELAPSED_S(a);
	} while (i < run_for);

	STOPWATCH_PRINT(a);

	double const elap = STOPWATCH_ELAPSED_F(a);
	int const elap_s = STOPWATCH_ELAPSED_S(a);
	int const elap_us = STOPWATCH_ELAPSED_US(a);
	
	struct timeval *const tval = (struct timeval *) malloc(sizeof(struct timeval));
	STOPWATCH_TO_TIMEVAL(*tval, a);

	printf("Elapsed in floating point value: %f\n", elap);
	printf("Elapsed seconds: %d%s\n", elap_s, elap_s != (int) elap ? " <== BUG!": "");
	printf("Elapsed micro seconds: %d%s\n", elap_us, elap_us != ((int) (elap * 1000000 + 0.5)) % 1000000 ? " <== BUG!": "");
	printf("Copy to timeval %ld.%06lu%s\n", tval->tv_sec, tval->tv_usec, tval->tv_sec != elap_s  ||  tval->tv_usec != elap_us ? " <== BUG!" : "");

	free(tval);
	return 0;
}
