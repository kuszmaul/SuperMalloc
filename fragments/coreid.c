/* Try to find the coreid using the cpuid instruction. */

#define _GNU_SOURCE
#include <assert.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
 
static void cpuid(unsigned info, unsigned *eax, unsigned *ebx, unsigned *ecx, unsigned *edx)
{
    __asm__(
        "cpuid;"                                            /* assembly code */
        :"=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx) /* outputs */
        :"a" (info), "c"(0)                                 /* input: info into eax */
                                                            /* clobbers: none */
    );
}

static int cpuid0(char *vendorid) {
  // requires vendorid is length 13 
  unsigned int a,b,c,d;
  cpuid(0, &a, &b, &c, &d);
  if (0) {
    printf("regs=0x%08x 0x%08x 0x%08x 0x%08x\n", a, b, c, d);
  }
  *(unsigned int*)(vendorid)   = b;
  *(unsigned int*)(vendorid+4) = d;
  *(unsigned int*)(vendorid+8) = c;
  vendorid[12] = 0;
  return a;
}

void test_coreid(void) {
  unsigned int a, b, c, d;
  cpuid(0xb, &a, &b, &c, &d);
  printf("coreid 0xb: regs=0x%08x 0x%08x 0x%08x 0x%08x\n", a, b, c, d);

  cpuid(4, &a, &b, &c, &d);
  printf("coreid 0x4: regs=0x%08x 0x%08x 0x%08x 0x%08x\n", a, b, c, d);
}

int coreid(void) {
  unsigned int a, b, c, d;
  cpuid(0xb, &a, &b, &c, &d);
  return d;
}

static __thread uint32_t cached_cpu, cached_cpu_count;
static uint32_t cached_getcpu(void) {
  if ((cached_cpu_count++)%2048  ==0) { cached_cpu = sched_getcpu(); if (0) printf("cpu=%d\n", cached_cpu); }
  return cached_cpu;
}


int main()
{
  int a, b;
 
  if (0)
  for (a = 0; a < 0xc; a++)
  {
    __asm__("cpuid"
            :"=a"(b)                 // EAX into b (output)
            :"0"(a)                  // a into EAX (input)
            :"%ebx","%ecx","%edx");  // clobbered registers
 
    printf("The code 0x%x gives %i\n", a, b);
  }
 
  char vendor[13];
  int v;
  v = cpuid0(vendor);
  printf("cpuid0 = 0x%x  vendor=%s\n", v, vendor);

  test_coreid();

#define C 8
#define COUNT 100000000
  {
    static int corecount[C];
    struct timespec start,end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < COUNT; i++) {
      int x = coreid();
      assert(x>=0 && x<C);
      corecount[x]++;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    for (int i = 0; i < C; i++) {
      printf(" %d", corecount[i]);
    }
    printf("\n");

    double diff = end.tv_sec - start.tv_sec + 1e-9*(end.tv_nsec - start.tv_nsec);
    printf("%.3fns/cpuid\n", 1e9*diff/COUNT);
  }
  {
    static int corecount[C];
    struct timespec start,end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < COUNT; i++) {
      int x = sched_getcpu();
      assert(x>=0 && x<C);
      corecount[x]++;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    for (int i = 0; i < C; i++) {
      printf(" %d", corecount[i]);
    }
    printf("\n");

    double diff = end.tv_sec - start.tv_sec + 1e-9*(end.tv_nsec - start.tv_nsec);
    printf("%.3fns/sched_getcpu()\n", 1e9*diff/COUNT);
  }
  {
    static int corecount[C];
    struct timespec start,end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < COUNT; i++) {
      int x = cached_getcpu();
      assert(x>=0 && x<C);
      corecount[x]++;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    for (int i = 0; i < C; i++) {
      printf(" %d", corecount[i]);
    }
    printf("\n");

    double diff = end.tv_sec - start.tv_sec + 1e-9*(end.tv_nsec - start.tv_nsec);
    printf("%.3fns/cached sched_getcpu()\n", 1e9*diff/COUNT);
  }

  return 0;
}
