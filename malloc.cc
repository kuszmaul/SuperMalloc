#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>

#include <algorithm>

#ifdef TESTING
#include <stdio.h>
#endif

#include "makehugepage.h"
#include "generated_constants.h"

#ifdef TESTING
static void test_size_2_bin(void) {
    for (size_t i=8; i<=largest_large; i++) {
        binnumber_t g = size_2_bin(i);
        assert(g<first_huge_bin_number);
        assert(i <= static_bin_info[g].object_size);
        if (g>0) assert(i > static_bin_info[g-1].object_size);
        else assert(g==0 && i==8);
	size_t s = bin_2_size(g);
	assert(s>=i);
	assert(size_2_bin(s) == g);
    }
    assert(size_2_bin(largest_large+1) == first_huge_bin_number);
    assert(size_2_bin(largest_large+4096) == first_huge_bin_number);
    assert(size_2_bin(largest_large+4096+1) == 1+first_huge_bin_number);
    assert(bin_2_size(first_huge_bin_number) == largest_large+4096);
    assert(bin_2_size(first_huge_bin_number+1) == largest_large+4096*2);
    assert(bin_2_size(first_huge_bin_number+2) == largest_large+4096*3);
    for (int k = 0; k < 1000; k++) {
      size_t s = chunksize * 10 + pagesize * k;
      binnumber_t b = size_2_bin(s);
      assert(size_2_bin(bin_2_size(b))==b);
      assert(bin_2_size(size_2_bin(s))==s);
    }

    // Verify that all the bins that are 256 or larger are multiples of a cache line.
    for (binnumber_t i = 0; i <= first_huge_bin_number; i++) {
      size_t os = static_bin_info[i].object_size;
      assert(os < 256 || os%64 == 0);
    }
}
#endif

struct chunk_info *chunk_infos;

void initialize_malloc(void) {
  const size_t n_elts = 1u<<27;
  const size_t alloc_size = n_elts * sizeof(chunk_info);
  const size_t n_chunks   = ceil(alloc_size, chunksize);
  chunk_infos = (chunk_info*)mmap_chunk_aligned_block(n_chunks);
  assert(chunk_infos);
}

union bitmap_chunk {
    unsigned char data[chunksize];
    bitmap_chunk  *next;
};
struct bitmaps {
    uint64_t next_free_byte;
    bitmap_chunk *bitmap_chunks;
} bitmaps;

unsigned char *allocate_bitmap(uint64_t n_bits) {
  uint64_t n_bytes = ceil(n_bits, 8);
    // This should be done atomically, and can be broken up in an interesting way.
    if (bitmaps.bitmap_chunks == 0
        || n_bytes + bitmaps.next_free_byte > chunksize) {
        // But this part can be factored out, since it's high-latency. If we end up with two of them, we should obtain our bitmap and free the new chunk.
        bitmap_chunk *c = (bitmap_chunk*)chunk_create();
        // then this must be done atomically, retesting that stuff
        c->next = bitmaps.bitmap_chunks;
        bitmaps.bitmap_chunks = c;
        bitmaps.next_free_byte = 0;
        if (0) printf("created chunk %p\n", c);
    }
    // Then this can be done atomically, assuming that there are enough bytes.
    if (n_bytes + bitmaps.next_free_byte > chunksize) return 0; 
    uint64_t o = bitmaps.next_free_byte;
    bitmaps.next_free_byte += n_bytes;
    unsigned char *result = &bitmaps.bitmap_chunks->data[o];
    return result;
};

#ifdef TESTING
static void test_bitmap(void) {
    const bool print = false;
    uint8_t *x = allocate_bitmap(100);
    if (print) printf("x       100=%p\n", x);
    uint8_t *y = allocate_bitmap(1);
    if (print) printf("y         1=%p\n", y);
    assert(x+ceil(100,8)==y);
    uint8_t *z = allocate_bitmap(1);
    if (print) printf("z         1=%p\n", z);
    assert(y+1==z);
    size_t s = 8* ((1<<21) - 107/8 - 2);
    uint8_t *w = allocate_bitmap(s);
    if (print) printf("w %9ld=%p\n", s, w);
    uint8_t *a = allocate_bitmap(3);
    if (print) printf("a=        3=%p\n", a);
    uint8_t *b = allocate_bitmap(1);
    if (print) printf("b         1=%p\n", b);
    uint8_t *c = allocate_bitmap(8ul<<21);
    if (print) printf("c   (8<<21)=%p\n", c);
    // This should fail.
    uint8_t *d = allocate_bitmap(1+(8ul<<21));
    if (print) printf("c 1+(8<<21)=%p\n", d);
    assert(d==0);
}
#endif

union page;

const uint64_t pointers_per_page = pagesize/sizeof(page*);

const uint64_t n_slots_per_page = (pagesize-64)/sizeof(uint64_t);

union page {
  struct page_in_use {
    uint64_t n_free_slots;
    uint64_t first_free_slot;
    page *next, *prev; // doubly linked list of pages with the same count
    union {
      uint8_t data[pagesize - 64];
      int64_t slots[n_slots_per_page];
    } __attribute__((aligned(64)));
  } piu;
  struct page_of_pages {
    uint64_t page_count;
    page   *next_page_of_pages;
    page   *pages[pointers_per_page-2];
  } pop;
  uint8_t raw_data[pagesize];
};
  

struct non_huge_bin {
  page *free_pages; // points at a page_of_pages, each subpage is just a page which may be all zeros (and may be madvise'd DONT_NEED).
  uint32_t fullest_page_index; // This is the minimum i such that partially_filled_page[i+1]!=NULL.  (Initially zero)
  page *partially_filled_pages[504]; // these are the doubly linked (pointing at headers).  [0] is the list of pages with no free slots, [1] is the list with 1 free slot.  Since the smallest objects is 8 bytes, and the page header uses some bytes, there are at most 504 free objects.
} bins[first_huge_bin_number];

static void add_chunk_to_bin(binnumber_t bin)
// Effect: Add a chunk to a bin (which is a non-huge bin).
{
  void *c = chunk_create();
  uint64_t chunknum = chunk_number_of_address(c);
  chunk_infos[chunknum].bin_number = bin;
  page *p = (page*)c;
  assert(chunksize / pagesize == pointers_per_page); // this happens to be true.
  p->pop.page_count = pointers_per_page-2;
  p->pop.next_page_of_pages = NULL;
  for (uint64_t i = 0; i < pointers_per_page - 2; i++) {
    p->pop.pages[i] = p+i+2;
  }
  p[1].pop.page_count = 0;
  p[1].pop.next_page_of_pages = p;
  bins[bin].free_pages = p+1;
}

static page *remove_free_page_from_bin(binnumber_t bin)
//  Effect: Assume there's a free page in the bin, remove a free page from the bin and return it.
{
  page *p = bins[bin].free_pages;
  uint64_t pc = p->pop.page_count;
  if (pc > 0) {
    page *r = p->pop.pages[pc-1];
    p->pop.page_count = pc-1;
    return r;
  } else {
    bins[bin].free_pages = p->pop.next_page_of_pages;
    return p;
  }
}

static void init_page_of_bin(binnumber_t bin, page *p)
// Effect: p is a previously uninitialized page that belongs in a particular bin.
//   Initialize it (including its free list), but don't put it into the partially_filled_pages array, since there isn't a slot
//   there for empty pages.
{
  uint32_t objsize = static_bin_info[bin].object_size;
  p->piu.n_free_slots = static_bin_info[bin].objects_per_page;
  p->piu.first_free_slot = objsize;
  for (uint64_t i = 0; i < n_slots_per_page; i += objsize) {
    p->piu.slots[i] = (i+1 < n_slots_per_page) ? i+1 : -1;
  }
}

static void* get_object_from_page_and_place_page_in_list(binnumber_t bin, page *p)
// Effect: p is a page with at least one free object in it.  Return
//  that object, and modify p so that the new object is removed from
//  the freelist.  Add p to the partially_filled_pages (unless p is completely full).
{
  uint64_t ffs = p->piu.first_free_slot;
  int64_t *result = &p->piu.slots[ffs];
  uint32_t new_free_slots = --p->piu.n_free_slots;
  p->piu.first_free_slot = *result;
  p->piu.prev = NULL;
  uint32_t old_fpi = bins[bin].fullest_page_index;
  uint32_t new_fpi;
  if (old_fpi == 0 || new_free_slots < old_fpi) {
    new_fpi = new_free_slots;
    printf("new_fpi=%d old_fpi=%d\n", new_free_slots, old_fpi);
    bins[bin].fullest_page_index = new_free_slots;
  } else {
    new_fpi = old_fpi;
  }
  page *old_head = bins[bin].partially_filled_pages[new_fpi-1];
  p->piu.next = old_head;
  if (old_head) {
    old_head->piu.prev = p;
  }
  bins[bin].partially_filled_pages[new_fpi-1] = p;
  return result;
}

static void* non_huge_malloc(size_t size) {
  binnumber_t bin = size_2_bin(size);
  assert(bin < first_huge_bin_number);
  int fpi = bins[bin].fullest_page_index;
  if (fpi != 0) {
    // There's partially full page.  Remove p from the partially filled pages where it lives now, and get an object from it.
    page *p = bins[bin].partially_filled_pages[fpi-1];
    bins[bin].partially_filled_pages[fpi-1] = p->piu.next;
    if (p->piu.next) {
      p->piu.next->piu.prev = NULL;
    }
    void *r = get_object_from_page_and_place_page_in_list(bin, p);
    return r;
  } else if (bins[bin].free_pages) {
 got_free_pages:
    page *p = remove_free_page_from_bin(bin);
    printf("got free page (%p)\n", p);
    init_page_of_bin(bin, p);
    void *o = get_object_from_page_and_place_page_in_list(bin, p);
    return o;
  } else {
    // There are no pages free, so allocate a chunk.
    add_chunk_to_bin(bin);
    goto got_free_pages;
  }
}

#ifdef TESTING
static void test_non_huge_malloc(void) {
  {
    void *v = non_huge_malloc(8);
    printf("m(8)=%p\n", v);
  }
  {
    void *v = non_huge_malloc(8);
    printf("m(8)=%p\n", v);
  }
  {
    void *v = non_huge_malloc(8);
    printf("m(8)=%p\n", v);
  }
  const int c = 100;
  void **a = (void**)non_huge_malloc(sizeof(void*)*c);
  printf("a=%p\n", a);
  for (int i = 0; i < c; i++) {
    a[i] = non_huge_malloc(80);
    printf("a[%d]=%p\n", i, a[i]);
  }
}
#endif

#if 0

struct into_page {
  into_page *next; // this is a pointer into the middle of a page.  We do arithmetic to get to the beginning of the page.
};
struct page_of_pages {
  page_of_pages *next;
  short n_pages_here;
  void *pages[510];
};
#endif

#if 0
struct bin {
  uint16_t object_size, objects_per_page;
  into_page **page_lists; // page_lists[0] has a list of pages that have 1 object free.  Total length page_lists[objects_per_page-1]
  // We don't bother to keep empty pages.
  // It really should be a dense list so we don't have to search through things.
  page_of_pages *empty_pages;
} bins[n_bins];

void* small_malloc(size_t size) {
  int32_t g = size_2_bin(size);
  uint16_t limit = bins[g].objects_per_page;
  into_page **pl = bins[g].page_lists;
  for (int i = 0; i < limit-1; i++) {
    if (pl[i]) {
      // Found an object.  Got to go to the bitmap, figure out which one to use next, queue it up, and then return pi[i]
      abort();
      return pl[i];
    }
  }
  abort();
}
#endif

#ifdef TESTING
static uint64_t slow_hyperceil(uint64_t a) {
  uint64_t r = 1;
  a--;
  while (a > 0) {
    a /= 2;
    r *= 2;
  }
  return r;
}

static void test_hyperceil_v(uint64_t a, uint64_t expected) {
  assert(hyperceil(a)==slow_hyperceil(a));
  if (expected) {
    assert(hyperceil(a)==expected);
  }
}

static void test_hyperceil(void) {
  test_hyperceil_v(1, 1);
  test_hyperceil_v(2, 2);
  test_hyperceil_v(3, 4);
  test_hyperceil_v(4, 4);
  test_hyperceil_v(5, 8);
  for (int i = 3; i < 27; i++) {
    test_hyperceil_v((1u<<i)+0,   (1u<<i));
    test_hyperceil_v((1u<<i)-1,   (1u<<i));
    test_hyperceil_v((1u<<i)+1, 2*(1u<<i));
  }
}

int main() {
  initialize_malloc();
  test_hyperceil();
  test_size_2_bin();
  test_chunk_create();
  test_bitmap();
  test_huge_malloc();
  if (0) test_non_huge_malloc();
}
#endif

// Three kinds of mallocs:
//   BIG, used for large allocations.  These are 2MB-aligned chunks.  We use BIG for anything bigger than a quarter of a chunk.
//   SMALL fit within a chunk.  Everything within a single chunk is the same size.
// The sizes are the powers of two (1<<X) as well as (1<<X)*1.25 and (1<<X)*1.5 and (1<<X)*1.75
extern "C" void *mymalloc(size_t size) {
    // We use a bithack to find the right size.
    if (size==0) return NULL;
    if (size<8)  size=8; // special case the very small objects.
    if (size>= 512*1024ul) {
        // huge case
      return huge_malloc(size);
    } else {
      abort();      
    }
}


// The basic idea of allocation, is that that we allocate 2MiB chunks
// (which are 2MiB aligned), and everything within a 2MiB chunk is the
// same size.

// For small objects: we keep meta-information at the beginning of
// each page:
//
//  * a free list (this is a 16 bits since the most objects per page
//    is less than 512.)
//
//  * a count of the number of free slots (also 16 bits, since most
//    objects per page is 512)
//
//  * two full pointers (to other pages) which can be used to
//    implement a heap (to get the pages with the fewest free slots)
//
// Therefore the overhead is 20 bytes.  Let's round to 64 bytes so
// that objects will be cache aligned to the extent possible.  (This
// choice costs us a few lost objects per page for the small sizes
// (objects of size 40 or smaller) So the objects size out as shown in
// the table below. We waste a few hundred bytes for per page for the
// last few objects (5% of the page).  
//
// Given that we waste on average half the difference between
// successive objets, for 256 to 320 we are wasting another 32 bytes
// per object on average, which is another 32*12 = 384 bytes wasted on
// average.  So the wasted 236 isn't too bad, since the total is 7%
// for that size.
//
// For medium objects, we fit as many as we can per page, and make the
// objects be 64-byte aligned.  Almost no space is wasted on these
// objects which range from 9 per page to 2 per page.
//
// Large objects are allocated as a multiple of page sizes.  The worst
// case is if you allocate a 4097 byte object.  You get two pages, for
// nearly a factor of 2x wastage.  We have one bin for each power of
// two: thus we have a 4KiB bin, an 8KiB bin, a 16KiB bin and so
// forth.  If you want a 12KiB object, we allocate it in a 16KiB bin,
// and rely on the last 4KiB not actually using resident memory.  We
// allocate this way so that we don't have to remember how many pages
// are allocated for an object or manage the fragmentation of those
// pages.  This means that we end up with 9 bins of large objects, for
// a total of 40 bins.
//
// Huge  objects are allocated as a multiple of the chunk size.
//
// Here are the non-huge sizes:
//
// make objsizes && ./objsizes 
// pagesize = 4096 bytes
// overhead = 64 bytes per page
// small objects:
// bin   objsize  objs/page  wastage
//   0         8      504       0
//   1        10      403       2
//   2        12      336       0
//   3        14      288       0
//   4        16      252       0
//   5        20      201      12
//   6        24      168       0
//   7        28      144       0
//   8        32      126       0
//   9        40      100      32
//  10        48       84       0
//  11        56       72       0
//  12        64       63       0
//  13        80       50      32
//  14        96       42       0
//  15       112       36       0
//  16       128       31      64
//  17       160       25      32
//  18       192       21       0
//  19       224       18       0
//  20       256       15     192
//  21       320       12     192
//  22       384       10     192
//  23       448        9       0
// medium objects:
//  24       504        8       0
//  25       576        7       0
//  26       672        6       0
//  27       806        5       2
//  28      1008        4       0
//  29      1344        3       0
//  30      2016        2       0
// large objects (page allocated):
//  31      1<<12
//  32      1<<13
//  33      1<<14
//  34      1<<15
//  35      1<<16
//  36      1<<17
//  37      1<<18
//  38      1<<19
//  39      1<<20
//
// We maintain a table which is simply the object size for each chunk.
// This is just a big array indexed by chunk number.  The chunk number
// is gotten by taking the chunk address and shifting it right by 21
// (sign extending) and adding an offset so that the index ranges from
// 0 (inclusive) to 2^{27} (exclusive).  The table contains a bin
// number, except for huge objects where it contains the number of
// chunks.  This allows the table to be kept with a single 32-bit
// number, making the entire table 2^{29} bytes (512MiB).  Again we
// rely on the table no being all mapped into main memory, but it
// might make good sense for this table to use transparent huge pages,
// even at the beginning, since it probably means a single page table
// entry for this table.

