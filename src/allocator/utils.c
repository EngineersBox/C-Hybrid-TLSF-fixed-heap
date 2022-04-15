#include "utils.h"

#if defined(__cplusplus)
#define htfh_decl inline
#else
#define htfh_decl
#endif

#if defined (__GNUC__) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)) && defined (__GNUC_PATCHLEVEL__)


htfh_decl int htfh_ffs(unsigned int word) {
    return __builtin_ffs(word) - 1;
}

htfh_decl int htfh_fls(unsigned int word) {
    const int bit = word ? 32 - __builtin_clz(word) : 0;
    return bit - 1;
}

#else
/* Fall back to generic implementation. */

htfh_decl int htfh_fls_generic(unsigned int word) {
    int bit = 32;
    if (!word) bit -= 1;
    if (!(word & 0xffff0000)) { word <<= 16; bit -= 16; }
    if (!(word & 0xff000000)) { word <<= 8; bit -= 8; }
    if (!(word & 0xf0000000)) { word <<= 4; bit -= 4; }
    if (!(word & 0xc0000000)) { word <<= 2; bit -= 2; }
    if (!(word & 0x80000000)) { word <<= 1; bit -= 1; }
    return bit;
}

#endif

/* Possibly 64-bit version of htfh_fls. */
#if defined (TLSF_64BIT)
htfh_decl int htfh_fls_sizet(size_t size) {
    int high = (int)(size >> 32);
    int bits = 0;
    if (high)
    {
        bits = 32 + htfh_fls(high);
    }
    else
    {
        bits = htfh_fls((int)size & 0xffffffff);

    }
    return bits;
}
#else
#define htfh_fls_sizet htfh_fls
#endif

#undef htfh_decl

/* This code has been tested on 32- and 64-bit (LP/LLP) architectures. */
htfh_static_assert(sizeof(int) * CHAR_BIT == 32);
htfh_static_assert(sizeof(size_t) * CHAR_BIT >= 32);
htfh_static_assert(sizeof(size_t) * CHAR_BIT <= 64);

/* SL_INDEX_COUNT must be <= number of bits in sl_bitmap's storage type. */
htfh_static_assert(sizeof(unsigned int) * CHAR_BIT >= SL_INDEX_COUNT);

/* Ensure we've properly tuned our sizes. */
htfh_static_assert(ALIGN_SIZE == SMALL_BLOCK_SIZE / SL_INDEX_COUNT);

size_t align_up(size_t x, size_t align) {
    if ((align & (align -1)) != 0) {
        set_alloc_errno(ALIGN_POWER_OF_TWO);
        return 0;
    }
    return (x + (align - 1)) & ~(align - 1);
}

size_t align_down(size_t x, size_t align) {
    if ((align & (align -1)) != 0) {
        set_alloc_errno(ALIGN_POWER_OF_TWO);
        return 0;
    }
    return x - (x & (align - 1));
}

void* align_ptr(const void* ptr, size_t align) {
    const  ptrdiff_t aligned = ((ptrdiff_t)(ptr) + (align - 1)) & ~(align - 1);
    if ((align & (align -1)) != 0) {
        set_alloc_errno(ALIGN_POWER_OF_TWO);
        return 0;
    }
    return (void*) aligned;
}

void mapping_insert(size_t size, int* fli, int* sli) {
    int fl, sl;
    if (size < SMALL_BLOCK_SIZE) {
        /* Store small blocks in first list. */
        fl = 0;
        sl = (int) size / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT);
    }
    else
    {
        fl = htfh_fls_sizet(size);
        sl = (int) (size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
        fl -= (FL_INDEX_SHIFT - 1);
    }
    *fli = fl;
    *sli = sl;
}

/* This version rounds up to the next block size (for allocations) */
void mapping_search(size_t size, int* fli, int* sli) {
    if (size >= SMALL_BLOCK_SIZE) {
        const size_t round = (1 << (htfh_fls_sizet(size) - SL_INDEX_COUNT_LOG2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}