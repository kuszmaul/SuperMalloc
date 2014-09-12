/**
 * \file   test-malloc_test.c
 * \author C. Lever and D. Boreham, Christian Eder ( ederc@mathematik.uni-kl.de )
 * \date   2000
 * \brief  Test file for xmalloc. This is a multi-threaded test system by 
 *         Lever and Boreham. It is first noted in their paper "malloc() 
 *         Performance in a Multithreaded Linux Environment", appeared at the
 *         USENIX 2000 Annual Technical Conference: FREENIX Track.
 *         This file is part of XMALLOC, licensed under the GNU General
 *         Public License version 3. See COPYING for more information.
 */
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
//#include "xmalloc-config.h"
//#include "xmalloc.h"

#include "../benchmarks/random.h"

#define CACHE_ALIGNED 1

#define xmalloc malloc
#define xfree free

#define	POOL_SIZE	4096
#define DEFAULT_OBJECT_SIZE 1024

int debug_flag = 0;
int verbose_flag = 0;
int num_workers = 4;
double run_time = 5.0;
int object_size = DEFAULT_OBJECT_SIZE;
/* array for thread ids */
pthread_t *thread_ids;
/* array for saving result of each thread */
struct counter {
  long c
#if CACHE_ALIGNED
 __attribute__((aligned(64)))
#endif
;
};
struct counter *counters;
/* memory pool used by all the threads */
void **mem_pool;

#define atomic_load(addr) __atomic_load_n(addr, __ATOMIC_CONSUME)
#define atomic_store(addr, v) __atomic_store_n(addr, v, __ATOMIC_RELEASE)

int done_flag = 0;
struct timeval begin;

static void
tvsub(tdiff, t1, t0)
	struct timeval *tdiff, *t1, *t0;
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

double elapsed_time(struct timeval *time0)
{
	struct timeval timedol;
	struct timeval td;
	double et = 0.0;

	gettimeofday(&timedol, (struct timezone *)0);
	tvsub( &td, &timedol, time0 );
	et = td.tv_sec + ((double)td.tv_usec) / 1000000;

	return( et );
}

static const long possible_sizes[] = {8,12,16,24,32,48,64,96,128,192,256,(256*3)/2,512, (512*3)/2, 1024, (1024*3)/2, 2048};
static const int n_sizes = sizeof(possible_sizes)/sizeof(long);

void *mem_allocator (void *arg)
{
	int i;	
	int thread_id = *(int *)arg;
	int start = POOL_SIZE * thread_id;
	int end = POOL_SIZE * (thread_id + 1);

        struct lran2_st lr;
	lran2_init(&lr, thread_id);

	if(verbose_flag>2) {
	  printf("Releaser %i works on memory pool %i to %i\n",
		thread_id, start, end);
	  printf("Releaser %i started...\n", thread_id);
	}

	while(!atomic_load(&done_flag)) {
	        bool did_one = false;
		/* find first NULL slot */
		for (i = start; i < end; ++i) {
		  if (NULL == atomic_load(&mem_pool[i])) {
		        did_one = true;
			size_t rand_size = possible_sizes[lran2(&lr)%n_sizes];
			size_t siz = object_size > 0 ? (size_t)object_size : rand_size;
		        atomic_store(&mem_pool[i], xmalloc(siz));
			if (debug_flag) 
			  printf("Allocate %i: slot %i\n", 
				thread_id, i);
			break;
		  }
		}
		if (!did_one) sched_yield();

	}
	pthread_exit(0);
}

void *mem_releaser(void *arg)
{
	int i;
	int loops = 0;
	int check_interval = 100;
	int thread_id = *(int *)arg;
	int start = POOL_SIZE * thread_id;
	int end = POOL_SIZE * (thread_id + 1);

	if(verbose_flag>2) {
	  printf("Allocator %i works on memory pool %i to %i\n",
		thread_id, start, end);
	  printf("Allocator %i started...\n", thread_id);
	}

	while(!atomic_load(&done_flag)) {
	        bool did_one = false;
		/* find non-NULL slot */
		for (i = start; i < end; ++i) {
		      void *ptr = atomic_load(&mem_pool[i]);
		      if (NULL != ptr) {
		          did_one = true;
			  atomic_store(&mem_pool[i], NULL);
			  xfree(ptr);
			  ++counters[thread_id].c;
			  if (debug_flag) 
			    printf("Releaser %i: slot %i\n", 
				thread_id, i);
			  break;
		   }
		}
		if (!did_one) sched_yield();
		++loops;
		if ( (0 == loops % check_interval) && 
		     (elapsed_time(&begin) > run_time) ) {
		        atomic_store(&done_flag, 1);
			break;
		}
	}
	pthread_exit(0);
}


int run_memory_free_test()
{
	void *ptr = NULL;
	int i;
	double elapse_time = 0.0;
	long total = 0;
	int *ids = (int *)xmalloc(sizeof(int) * num_workers);

	/* Initialize counter */
	for(i = 0; i < num_workers; ++i) 
		counters[i].c = 0;

	/* Initialize memory pool */
	for (i = 0; i < POOL_SIZE * num_workers; ++i)
		mem_pool[i] = NULL;

	gettimeofday(&begin, (struct timezone *)0);

	/* Start up the mem_allocator and mem_releaser threads  */
	for(i = 0; i < num_workers; ++i) {
		ids[i] = i;
		if (verbose_flag) printf("Starting mem_releaser %i ...\n", i);
		if (pthread_create(&thread_ids[i * 2], NULL, mem_releaser, (void *)&ids[i])) {
			perror("pthread_create mem_releaser");
			exit(errno);
		}

		if (verbose_flag) printf("Starting mem_allocator %i ...\n", i);
		if (pthread_create(&thread_ids[i * 2 + 1], NULL, mem_allocator, (void *)&ids[i])) {
			perror("pthread_create mem_allocator");
			exit(errno);
		}
	}

	if (verbose_flag) printf("Testing for %.2f seconds\n\n", run_time);

	for(i = 0; i < num_workers * 2; ++i)
		pthread_join (thread_ids[i], &ptr);

	elapse_time = elapsed_time (&begin);

	for(i = 0; i < num_workers; ++i) {
		if (verbose_flag) {
			printf("Thread %2i frees %ld blocks in %.2f seconds. %.2f free/sec.\n",
			       i, counters[i].c, elapse_time, ((double)counters[i].c/elapse_time));
		}
	}
	if (verbose_flag) printf("----------------------------------------------------------------\n");
	for(i = 0; i < num_workers; ++i) total += counters[i].c;
	if (verbose_flag)
	  printf("Total %ld freed in %.2f seconds. %.2fM free/second\n",
		 total, elapse_time, ((double) total/elapse_time)*1e-6);
	else
	  printf("%.0f\n", (double)total/elapse_time);

	if (verbose_flag) printf("Program done\n");
	return(0);
}

void usage(char *prog)
{
	printf("%s [-w workers] [-t run_time] [-d] [-v]\n", prog);
	printf("\t -w number of set of allocator and freer, default 2\n");
	printf("\t -t run time in seconds, default 20.0 seconds.\n");
	printf("\t -s size of object to allocate (default %d bytes) (specify -1 to get many different object sizes)\n", DEFAULT_OBJECT_SIZE);
	printf("\t -d debug mode\n");
	printf("\t -v verbose mode (-v -v produces more verbose)\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "w:t:ds:v")) != -1) {
		
		switch (c) {

		case 'w':
			num_workers = atoi(optarg);
			break;
		case 't':
			run_time = atof(optarg);
			break;
		case 'd':
			debug_flag = 1;
			break;
		case 's':
			object_size = atoi(optarg);
			break;
		case 'v':
			verbose_flag++;
			break;
		default:
			usage(argv[0]);
		}
	}

	/* allocate memory for working arrays */
	thread_ids = (pthread_t *) xmalloc(sizeof(pthread_t) * num_workers * 2);
	counters = (struct counter *) xmalloc(sizeof(*counters) * num_workers);
	mem_pool  = (void **) (CACHE_ALIGNED
			       ? aligned_alloc(64, sizeof(void *) * POOL_SIZE * num_workers)
			       : malloc(sizeof(void *) * POOL_SIZE * num_workers));
	    
	
	run_memory_free_test();
	
	return 0;
}
