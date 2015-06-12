/* -*- Mode: C; c-basic-offset: 2 -*- */
/*
 * Copyright (c) 2015, Logan P. Evans <loganpevans@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <random>
#include <thread>
#include <sys/resource.h>
#include <unistd.h>

#define START_NUM_THREADS 1
#define MAX_NUM_THREADS 50
// 2.397 * 2**30 is roughly one second (2.4 GHz).
static uint64_t CYCLES_PER_TRIAL =
    (uint64_t)(5UL * 2.397 * 1024 * 1024 * 1024);
static uint64_t PERMITTED_BYTES = 1UL * 1024 * 1024 * 1024;

static bool CONSTANT_SPACE_PER_TRIAL[] = {true, false};

static uint64_t BYTES_PER_MALLOC[] = {
  8UL,  // tiny
  512UL,  // quantum-spaced
  1024UL,  // sub-page
  4UL * 1024,  // large
  1UL * 1024 * 1024,  // large
  2UL * 1024 * 1024,  // huge
};

static uint64_t rdtsc()
{
  uint32_t hi, lo;

  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return (((uint64_t)lo) | (((uint64_t)hi) << 32));
}

std::mutex counter_mutex;
uint64_t total_num_mallocs, total_malloc_cycles;
uint64_t total_num_frees,   total_free_cycles;

static void thread_func(int threadnum,
			uint64_t end_timestamp,
			uint64_t bytes_per_malloc,
			uint64_t bytes_per_thread) {

  unsigned int buffer_size;
  std::default_random_engine generator(threadnum+1); // Want different threads to do different things.
  std::uniform_int_distribution<int> *distribution;

  uint64_t num_mallocs   = 0;
  uint64_t num_frees     = 0;
  uint64_t malloc_cycles = 0;
  uint64_t free_cycles   = 0;

  buffer_size = (unsigned int)(bytes_per_thread / bytes_per_malloc);
  distribution = new std::uniform_int_distribution<int>(
      0, (int)buffer_size - 1);

  char **buffer = new char*[buffer_size];
  for (unsigned int idx = 0; idx < buffer_size; idx++) {
    buffer[idx] = NULL;
  }

  while (rdtsc() < end_timestamp) {
    int malloc_idx = (*distribution)(generator);
    if (buffer[malloc_idx] == NULL) {
      uint64_t start_timestamp = rdtsc();
      buffer[malloc_idx] = (char *)malloc(bytes_per_malloc);
      buffer[malloc_idx][0] = 1; // Touch the object
      uint64_t stop_timestamp = rdtsc();
      num_mallocs++;
      malloc_cycles += stop_timestamp - start_timestamp;
    } else {
      uint64_t start_timestamp = rdtsc();
      free(buffer[malloc_idx]);
      uint64_t stop_timestamp = rdtsc();
      buffer[malloc_idx] = NULL;
      num_frees++;
      free_cycles += stop_timestamp - start_timestamp;
    }
  }

  for (unsigned int idx = 0; idx < buffer_size; idx++) {
    free(buffer[idx]);
  }

  {
    std::lock_guard<std::mutex> lock(counter_mutex);
    total_num_mallocs   += num_mallocs;
    total_malloc_cycles += malloc_cycles;
    total_num_frees     += num_frees;
    total_free_cycles   += free_cycles;
  }
  delete distribution;
  delete [] buffer;
}

uint64_t current_rss() {
  FILE* fp = NULL;
  if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
    return (size_t)0L;		/* Can't open? */
  long rss = 0;
  if ( fscanf( fp, "%*s%ld", &rss ) != 1 ) {
    fclose( fp );
    return (size_t)0L;		/* Can't read? */
  }
  fclose( fp );
  return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);
}

int main() {
  std::thread threads[MAX_NUM_THREADS];
  uint64_t end_timestamp;
  uint64_t bytes_per_malloc;
  uint64_t bytes_per_thread;
  bool constant_space_per_trial;

  printf(
      "threads,cycles_per_malloc,num_mallocs,total_malloc_cycles,"
      "cycles_per_free,num_frees,total_free_cycles,"
      "bytes_per_malloc,bytes_per_thread,constant_space_per_trial\n");

  for (unsigned int space_idx = 0;
      space_idx < sizeof(CONSTANT_SPACE_PER_TRIAL) /
                  sizeof(CONSTANT_SPACE_PER_TRIAL[0]);
      space_idx++) {
    constant_space_per_trial = CONSTANT_SPACE_PER_TRIAL[space_idx];
    for (unsigned int bytes_idx = 0;
         bytes_idx < sizeof(BYTES_PER_MALLOC) / sizeof(BYTES_PER_MALLOC[0]);
         bytes_idx++) {
      bytes_per_malloc = BYTES_PER_MALLOC[bytes_idx];

      for (unsigned int num_threads = START_NUM_THREADS;
           num_threads < MAX_NUM_THREADS; num_threads*=2) {
        end_timestamp = rdtsc() + CYCLES_PER_TRIAL;
        if (constant_space_per_trial)
          bytes_per_thread = PERMITTED_BYTES / num_threads;
        else
          bytes_per_thread = PERMITTED_BYTES / MAX_NUM_THREADS;

	fprintf(stderr, "rss=%.0fM\n", current_rss()/(1024.0*1024.0));

        for (unsigned int idx = 0; idx < num_threads; idx++) {
	  threads[idx] = std::thread(thread_func,
				     idx,
				     end_timestamp, bytes_per_malloc, bytes_per_thread);
        }

        for (unsigned int idx = 0; idx < num_threads; idx++) {
          threads[idx].join();
        }
        printf("%2d,%5lu,%10lu,%12lu,%5lu,%10lu,%12lu,%lu,%10lu,%d\n",
               num_threads,
	       total_num_mallocs > 0 ? total_malloc_cycles / total_num_mallocs : -1, total_num_mallocs, total_malloc_cycles,
	       total_num_frees   > 0 ? total_free_cycles   / total_num_frees   : -1, total_num_frees,   total_free_cycles,
               bytes_per_malloc, bytes_per_thread, constant_space_per_trial);
      }
    }
  }

  fprintf(stderr, "rss=%.0fM\n", current_rss()/(1024.0*1024.0));

  return 0;
}

