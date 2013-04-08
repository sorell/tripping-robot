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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>



static pthread_t *thread;
static int numberOfThreads;
// static const int numberOfThreads = (int) sizeof(thread) / (int) sizeof(thread[0]);

static int running = 1;

static void *
writerThread(void *data)
{
	// int const threadNum = (int) (size_t) data;
	FILE *filp = fopen("/proc/rt_mod_test", "w");

	if (!filp) {
		fprintf(stderr, "Can't open file\n");
		return NULL;
	}

	
	while (running) {
		fprintf(filp, "a");
		fflush(filp);
		usleep(10);
		// sleep(100);
		// break;
	}

	fclose(filp);
	
	return NULL;
}



void sighandler(int s)
{
	switch (s) {
	case SIGINT:
		running = 0;
	default:
	break;
	}
}


int
main(void)
{
	int i;

	if ((numberOfThreads = sysconf(_SC_NPROCESSORS_ONLN)) < 1) {
		numberOfThreads = 1;
	}

	thread = (pthread_t *) malloc(sizeof(pthread_t) * numberOfThreads);

	signal(SIGINT, sighandler);

	for (i=0; i<numberOfThreads; ++i) {
		// ThreadReady[i] = false;
		
		fprintf(stderr, "Starting %d\n", i);
	
		if (pthread_create(&thread[i], NULL, writerThread, (void *) (long long) i) ) {
			fprintf(stderr, "Failed to pthread: %s\n", strerror(errno));
			exit(1);
		}
	}
	
	// for (i=0; i<numberOfThreads; ++i) 
	// {
		// while (!ThreadReady[i]);
	// }
	
	for (i=0; i<numberOfThreads; ++i) {
		pthread_join(thread[i], NULL);
	}

	free(thread);

	return 0;
}