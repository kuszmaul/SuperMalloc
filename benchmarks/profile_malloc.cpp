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


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <random>

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

struct thread_func_args {
  uint64_t end_timestamp;
  uint64_t bytes_per_malloc;
  uint64_t bytes_per_thread;
  uint64_t num_mallocs_out;
  uint64_t total_cycles_out;
};

static void *
thread_func(void *args) {
  uint64_t end_timestamp;
  uint64_t bytes_per_malloc;
  uint64_t bytes_per_thread;
  uint64_t num_mallocs_out;
  uint64_t total_cycles_out;

  uint64_t start_timestamp, stop_timestamp;
  char **buffer;
  unsigned int buffer_size;
  unsigned int idx;
  int malloc_idx;
  std::default_random_engine generator;
  std::uniform_int_distribution<int> *distribution;

  end_timestamp = ((struct thread_func_args *)args)->end_timestamp;
  bytes_per_thread = ((struct thread_func_args *)args)->bytes_per_thread;
  bytes_per_malloc = ((struct thread_func_args *)args)->bytes_per_malloc;
  num_mallocs_out = 0;
  total_cycles_out = 0;

  buffer_size = (unsigned int)(bytes_per_thread / bytes_per_malloc);
  distribution = new std::uniform_int_distribution<int>(
      0, (int)buffer_size - 1);

  buffer = (char **)malloc(sizeof(char*) * buffer_size);
  for (idx = 0; idx < buffer_size; idx++) {
    buffer[idx] = NULL;
  }

  while (rdtsc() < end_timestamp) {
    malloc_idx = (*distribution)(generator);
    if (buffer[malloc_idx] == NULL) {
      start_timestamp = rdtsc();
      buffer[malloc_idx] = (char *)malloc(bytes_per_malloc);
      stop_timestamp = rdtsc();
      num_mallocs_out++;
      total_cycles_out += stop_timestamp - start_timestamp;
    } else {
      free(buffer[malloc_idx]);
      buffer[malloc_idx] = NULL;
    }
  }

  for (idx = 0; idx < buffer_size; idx++) {
    free(buffer[idx]);
  }

  ((struct thread_func_args *)args)->num_mallocs_out = num_mallocs_out;
  ((struct thread_func_args *)args)->total_cycles_out = total_cycles_out;

  delete distribution;
  free(buffer);
  return (void *)NULL;
}

int main() {
  pthread_t threads[MAX_NUM_THREADS];
  struct thread_func_args args[MAX_NUM_THREADS];
  uint64_t end_timestamp;
  uint64_t bytes_per_malloc;
  uint64_t bytes_per_thread;
  uint64_t num_mallocs;
  uint64_t total_cycles;
  bool constant_space_per_trial;

  printf(
      "threads,cycles_per_malloc,num_mallocs,total_cycles,"
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
           num_threads < MAX_NUM_THREADS; num_threads++) {
        end_timestamp = rdtsc() + CYCLES_PER_TRIAL;
        if (constant_space_per_trial)
          bytes_per_thread = PERMITTED_BYTES / num_threads;
        else
          bytes_per_thread = PERMITTED_BYTES / MAX_NUM_THREADS;

        for (unsigned int idx = 0; idx < num_threads; idx++) {
          args[idx].end_timestamp = end_timestamp;
          args[idx].bytes_per_thread = bytes_per_thread;
          args[idx].bytes_per_malloc = bytes_per_malloc;
          pthread_create(
            &threads[idx], NULL, (void *(*)(void *))thread_func, &args[idx]);
        }

        num_mallocs = 0;
        total_cycles = 0;
        for (unsigned int idx = 0; idx < num_threads; idx++) {
          pthread_join(threads[idx], NULL);
          num_mallocs += args[idx].num_mallocs_out;
          total_cycles += args[idx].total_cycles_out;
        }
        printf("%d,%lu,%lu,%lu,%lu,%lu,%d\n",
               num_threads, num_mallocs > 0 ? total_cycles / num_mallocs : -1, num_mallocs, total_cycles,
               bytes_per_malloc, bytes_per_thread, constant_space_per_trial);
      }
    }
  }

  return 0;
}

