
union chunk_union {
  union chunk_union *next;
  char chunk_data[chunksize];
};

struct the_chunk_list { // Put these in a struct together so they'll be on the same cache line.
  volatile unsigned int       lock __attribute((__aligned__(64)));
  union chunk_union *list;
};
static struct the_chunk_list tcl = {0,0};

struct cc_data {
  union chunk_union *u;
};

static void prepop_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data*)cc_data_v;
  cc->u = tcl.list; // we want to write into the return result, and we want the list to be in cache.
  // But it's not good enough to simply do   __builtin_prefetch(&cc->u,    1, 3)
  // But we do want to convert the list to writeable if possible.
  __builtin_prefetch(&tcl.list, 1, 3); // we want to do a write (and a read) onto the chunk list.
}


static void pop_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data*)cc_data_v;
  union chunk_union *head = tcl.list;
  cc->u = head;
  if (head) {
    tcl.list  = head->next;
  }
}


static void *chunk_get_from_pool(void) {
  struct cc_data d;
  atomically(&tcl.lock, prepop_chunk, pop_chunk, &d);
  return d.u;
}
 


static void prepush_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data*)cc_data_v;
  cc->u->next = tcl.list;
  __builtin_prefetch(&tcl.list, 1, 3); // we want to do a write (and a read) onto the chunk list.
}

static void push_chunk(void *cc_data_v) {
  cc_data *cc = (cc_data *)cc_data_v;
  cc->u->next = tcl.list;
  tcl.list    = cc->u;
}


void return_chunk_to_pool(void *chunk) {
  struct cc_data d = {(chunk_union *)chunk};
  atomically(&tcl.lock, prepush_chunk, push_chunk, &d);
}


static void *chunk_create(void) {
  while (1) {
    void *p = chunk_get_from_pool();
    if (p) return p;
    chunk_create_into_pool();
  }
}

static void chunk_create_into_pool(void) {
  // Need to create a chunk
  return_chunk_to_pool(mmap_chunk_aligned_block(1));
}

#ifdef TESTING
void test_chunk_create(void) {
  {
    void *p = chunk_get_from_pool();
    bassert(p==0);
  }
  {
    chunk_create_into_pool();
    void *p = chunk_get_from_pool();
    bassert(p!=0);
    long pl = (long)p;
    bassert(pl%chunksize == 0);
  }
}
#endif
