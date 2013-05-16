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
#include <pthread.h>
#include <signal.h>
#include "../runtrace.h"


static int running = 1;

static pthread_t *thread;
static int numberOfThreads;


static void *
writerThread(void *data)
{
	// char buf[RT_MSG_MAX];
	int i = 0;
	int const threadNum = (int) (size_t) data;

	fprintf(stderr, "Starting %d\n", threadNum);
	
	while (running) {
		// snprintf(buf, sizeof(buf), "Thread %d, write %d", threadNum, i++);
		// TP_FUNK(buf);
		TP_FILE_P("Thread %d, write %d", threadNum, i++);
		// usleep(20);
		
	}
	
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
	

	if ((numberOfThreads = sysconf(_SC_NPROCESSORS_ONLN) * 2) < 1) {
	// if ((numberOfThreads = sysconf(_SC_NPROCESSORS_ONLN) - 1) < 1) {
		numberOfThreads = 1;
	}

	// numberOfThreads = 5;
	thread = (pthread_t *) malloc(sizeof(pthread_t) * numberOfThreads);

	signal(SIGINT, sighandler);

	runtrace_flush_on_abort(1);
	runtrace_init(numberOfThreads+1);

	for (i=0; i<numberOfThreads; ++i) {
		
		if (pthread_create(&thread[i], NULL, writerThread, (void *) (long long) i) ) {
			fprintf(stderr, "Failed to pthread: %s\n", strerror(errno));
			exit(1);
		}
	}
	
	
	i = 0;
	// int size = 4;
	while (running) {
		// if (i % 5 == 0) {
		// 	runtrace_reconfigure(size);
		// 	size <<= 1;
		// }
		++i;

		
		usleep(1000000);
		print_trace_stack(RT_PRINT_ALL);

		if (5 == i) {
			// abort();
			// running = 0;
		}
		
	}
	
	for (i=0; i<numberOfThreads; ++i) {
		pthread_join(thread[i], NULL);
	}
	
	
	
	
	// for (i=0; i<257; ++i) {
		// snprintf(buf, sizeof(buf), "Tracepoint %d", cnt++);
		// TP_FUNK(buf);
		// usleep(200);
	// }
	// print_trace_stack(RT_PRINT_ALL | RT_PRINT_TIME_MS);
	// print_trace_stack(RT_PRINT_DEFAULT);
	runtrace_exit();

	free(thread);
	
	return 0;
}


