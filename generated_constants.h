#ifndef GENERATED_CONSTANTS_H
#define GENERATED_CONSTANTS_H
static const size_t slot_size = 1;
static const int page_size = 4096;
typedef uint32_t binnumber_t;
static struct { uint32_t object_size; uint32_t objects_per_page; } static_bin_info[] __attribute__((unused)) = {
// pagesize = 4096 bytes
// overhead = 64 bytes per page
// small objects:
  /*objsize,*/ //   bin   wastage
 {   8, 504},  //     0        0
 {  10, 403},  //     1        2
 {  12, 336},  //     2        0
 {  14, 288},  //     3        0
 {  16, 252},  //     4        0
 {  20, 201},  //     5       12
 {  24, 168},  //     6        0
 {  28, 144},  //     7        0
 {  32, 126},  //     8        0
 {  40, 100},  //     9       32
 {  48,  84},  //    10        0
 {  56,  72},  //    11        0
 {  64,  63},  //    12        0
 {  80,  50},  //    13       32
 {  96,  42},  //    14        0
 { 112,  36},  //    15        0
 { 128,  31},  //    16       64
 { 160,  25},  //    17       32
 { 192,  21},  //    18        0
 { 224,  18},  //    19        0
 { 256,  15},  //    20      192
 { 320,  12},  //    21      192
 { 384,  10},  //    22      192
 { 448,   9},  //    23        0
// medium objects:
 { 504,   8},  //    24         0
 { 576,   7},  //    25         0
 { 672,   6},  //    26         0
 { 806,   5},  //    27         2
 {1008,   4},  //    28         0
 {1344,   3},  //    29         0
 {2016,   2},  //    30         0
// large objects (page allocated):
 {1ul<<12, 1}, //    31
 {1ul<<13, 1}, //    32
 {1ul<<14, 1}, //    33
 {1ul<<15, 1}, //    34
 {1ul<<16, 1}, //    35
 {1ul<<17, 1}, //    36
 {1ul<<18, 1}, //    37
 {1ul<<19, 1}, //    38
 {1ul<<20, 1}, //    39
// huge objects (chunk allocated) start  at this size.
 {1ul<<21, 1}};//  40
static const size_t largest_small         = 448;
static const size_t largest_medium        = 2016;
static const size_t largest_large         = 1048576;
static const size_t chunk_size            = 2097152;
static const binnumber_t first_medium_bin_number = 24;
static const binnumber_t first_large_bin_number = 31;
static const binnumber_t first_huge_bin_number   = 40;
#endif
