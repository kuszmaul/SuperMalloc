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
 *
 * This verison does a fixed amount of work rather than running for a fixed amount of time.
 */
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "../benchmarks/random.h"

#define CACHE_ALIGNED 1

#define xmalloc malloc
#define xfree free

#define DEFAULT_OBJECT_SIZE 1024

int debug_flag = 0;
int verbose_flag = 0;
int num_workers = 4;
int per_thread_work_count = 20 * 1024 * 1024; // how many mallocs and frees should each thread do?
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

#define OBJECTS_PER_BATCH 4096
struct batch {
  void *objects[OBJECTS_PER_BATCH];
  int   objcount;
};

#define BATCH_COUNT_LIMIT 100
struct batchbag {
  struct batch *batches[BATCH_COUNT_LIMIT];
  int n_in_bag;
  int n_producers_remaining;
  pthread_cond_t empty_cv, full_cv;
  pthread_mutex_t lock;
} batchbag = {
  .n_in_bag = 0,
  .empty_cv = PTHREAD_COND_INITIALIZER,
  .full_cv  = PTHREAD_COND_INITIALIZER,
  .lock     = PTHREAD_MUTEX_INITIALIZER
};

#define atomic_load(addr) __atomic_load_n(addr, __ATOMIC_CONSUME)
#define atomic_store(addr, v) __atomic_store_n(addr, v, __ATOMIC_RELEASE)

static void enqueue_batch(struct batch *batch) {
  pthread_mutex_lock(&batchbag.lock);
  // Wait until the bag has few enough items in it.
  while (atomic_load(&batchbag.n_in_bag) >= BATCH_COUNT_LIMIT) {
    pthread_cond_wait(&batchbag.full_cv, &batchbag.lock);
  }
  batchbag.batches[batchbag.n_in_bag++] = batch;
  pthread_cond_signal(&batchbag.empty_cv);
  pthread_mutex_unlock(&batchbag.lock);
}

static void initialize_batchbag(int n_producers) {
  assert(batchbag.n_in_bag == 0);
  batchbag.n_producers_remaining = n_producers;
}

static void close_batchbag_producer(void) {
  pthread_mutex_lock(&batchbag.lock);
  assert(batchbag.n_producers_remaining > 0);
  batchbag.n_producers_remaining--;
  pthread_cond_signal(&batchbag.empty_cv);
  pthread_mutex_unlock(&batchbag.lock);
}

static struct batch* dequeue_batch() {
  // Return a batch, or NULL if the queue has been closed
  pthread_mutex_lock(&batchbag.lock);
  while (atomic_load(&batchbag.n_in_bag) == 0
	 &&
	 atomic_load(&batchbag.n_producers_remaining) > 0) {
    //printf(" n_in_bag=%d, n_producers_remaining=%d\n", batchbag.n_in_bag, batchbag.n_producers_remaining);
    pthread_cond_wait(&batchbag.empty_cv, &batchbag.lock);
  }
  int n_in_bag = atomic_load(&batchbag.n_in_bag);
  if (n_in_bag > 0) {
    batchbag.n_in_bag = n_in_bag-1;
    struct batch *result = batchbag.batches[n_in_bag-1];
    pthread_cond_signal(&batchbag.full_cv);
    pthread_mutex_unlock(&batchbag.lock);
    return result;
  } else {
    assert(atomic_load(&batchbag.n_producers_remaining) == 0);
    pthread_cond_signal(&batchbag.empty_cv); // make sure everyone else got woken up.
    pthread_mutex_unlock(&batchbag.lock);
    return NULL;
  }
}   

static void *mem_allocator (void *arg) {
  int thread_id = *(int *)arg;
  struct lran2_st lr;
  lran2_init(&lr, thread_id);

  int ocount = 0;
  while (1) {
    struct batch *b = xmalloc(sizeof(*b));
    b->objcount = 0;
    for (int i = 0; i < OBJECTS_PER_BATCH && ocount < per_thread_work_count; i++) {
      size_t siz = object_size > 0 ? object_size : possible_sizes[lran2(&lr)%n_sizes];
      b->objects[i] = xmalloc(siz);
      memset(b->objects[i], i%256, siz);
      b->objcount++;
      ocount++;
      if (ocount >= per_thread_work_count) break;
    }
    //printf("ocount=%d per_thread_work_count=%d\n", ocount, per_thread_work_count);
    enqueue_batch(b);
    if (ocount >= per_thread_work_count) break;
  }
  close_batchbag_producer();
  if (verbose_flag) printf("mem_allocator %d finishing\n", thread_id);
  return NULL;
}

static void *mem_releaser(void *arg) {
  int thread_id = *(int *)arg;

  while (1) {
    if (verbose_flag) printf("%s:%d %d dequeuing\n", __FILE__, __LINE__, thread_id);
    struct batch *b = dequeue_batch();
    if (verbose_flag) printf("%s:%d %d got %p\n", __FILE__, __LINE__, thread_id,b);
    if (b) {
      for (int i = 0; i < b->objcount; i++) {
	xfree(b->objects[i]);
      }
      counters[thread_id].c += b->objcount;
      xfree(b);
    } else {
      return NULL;
    }
  }
}

int run_memory_free_test()
{
	void *ptr = NULL;
	int i;
	double elapse_time = 0.0;
	long total = 0;
	int *ids = (int *)xmalloc(sizeof(int) * num_workers);

	initialize_batchbag(num_workers);

	/* Initialize counter */
	for(i = 0; i < num_workers; ++i) 
		counters[i].c = 0;

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

	if (verbose_flag) printf("Testing for %d allocations per thread\n\n", per_thread_work_count);

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
	printf("\t -w number of producer threads (and number of consumer threads), default %d\n", num_workers);
	printf("\t -t run time in seconds, default 20.0 seconds.\n");
	printf("\t -s size of object to allocate (default %d bytes) (specify -1 to get many different object sizes)\n", DEFAULT_OBJECT_SIZE);
	printf("\t -d debug mode\n");
	printf("\t -v verbose mode (-v -v produces more verbose)\n");
	exit(1);
}

int main(int argc, char **argv)
{
	int c;
	while ((c = getopt(argc, argv, "w:n:ds:v")) != -1) {
		
		switch (c) {

		case 'w':
			num_workers = atoi(optarg);
			break;
		case 'n':
		        per_thread_work_count = atof(optarg);
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
	
	run_memory_free_test();
	assert(batchbag.n_in_bag == 0);
	return 0;
}
