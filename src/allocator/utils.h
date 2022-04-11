#pragma once

#ifndef _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_UTILS_
#define _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_UTILS_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include "../error/allocator_errno.h"

static size_t align_up(size_t x, size_t align) {
    if ((align & (align -1)) != 0) {
        set_alloc_errno(ALIGN_POWER_OF_TWO);
        return 0;
    }
    return (x + (align - 1)) & ~(align - 1);
}

static size_t align_down(size_t x, size_t align) {
    if ((align & (align -1)) != 0) {
        set_alloc_errno(ALIGN_POWER_OF_TWO);
        return 0;
    }
    return x - (x & (align - 1));
}

static void* align_ptr(const void* ptr, size_t align) {
    const  ptrdiff_t aligned = ((ptrdiff_t)(ptr) + (align - 1)) & ~(align - 1);
    if ((align & (align -1)) != 0) {
        set_alloc_errno(ALIGN_POWER_OF_TWO);
        return 0;
    }
    return (void*) aligned;
}

#ifdef __cplusplus
};
#endif

#endif // _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_UTILS_
