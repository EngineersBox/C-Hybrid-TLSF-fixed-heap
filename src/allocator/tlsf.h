#pragma once

#ifndef _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_
#define _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include "../thread/lock.h"

#define CLASS_ELEMENTS_SIZE sizeof(unsigned)
typedef void* heap_mem_pool_t;

typedef struct Allocator {
    __htfh_lock_t mutex;

    unsigned fl_bitmask;
    void* fl_classes[CLASS_ELEMENTS_SIZE];

    size_t heap_size;
    heap_mem_pool_t heap;
} Allocator;

typedef Allocator* allocator_ptr_t;

#ifdef __cplusplus
};
#endif

#endif // _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_