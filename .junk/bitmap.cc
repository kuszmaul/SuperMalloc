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
    bassert(x+ceil(100,8)==y);
    uint8_t *z = allocate_bitmap(1);
    if (print) printf("z         1=%p\n", z);
    bassert(y+1==z);
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
    bassert(d==0);
}
#endif
