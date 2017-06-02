/* Determine if we have tsx */
#include <cassert>
#include <cstdint>
#include <cstdio>

bool have_TSX() {
    bool result = false;
    const int hle_ebx_mask = 1<<4;
    const int rtm_ebx_mask = 1<<11;

    int32_t reg_ebx = 0;
    int32_t reg_eax = 7;
    int32_t reg_ecx = 0;
    __asm__ __volatile__ ( "movl %%ebx, %%esi\n"
                           "cpuid\n"
                           "movl %%ebx, %0\n"
                           "movl %%esi, %%ebx\n"
                           : "=a"(reg_ebx) : "0" (reg_eax), "c" (reg_ecx) : "esi",
// should be a test for x86-64
#if 1
                           "ebx",
#endif
                           "edx"
                           );
    result = (reg_ebx & hle_ebx_mask)!=0 ;
if( result ) assert( (reg_ebx & rtm_ebx_mask)!=0 );
    return result;
}

#if 0
int main () {
  printf("have_TSX=%d\n", have_TSX());
  return 0;
}
#endif
