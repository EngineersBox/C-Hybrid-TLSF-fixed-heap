#pragma once

#ifndef _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_
#define _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include "../thread/lock.h"
#include "controller.h"

typedef void* heap_mem_pool_t;

typedef struct Allocator {
    __htfh_lock_t mutex;

    Controller* controller;

    size_t heap_size;
    heap_mem_pool_t heap;
} Allocator;

int htfh_new(Allocator* alloc);
int htfh_init(Allocator* alloc, size_t heap_size)  __attribute__((nonnull));
int htfh_destruct(Allocator* alloc)  __attribute__((nonnull));

heap_mem_pool_t htfh_add_pool(Allocator* alloc, heap_mem_pool_t mem, size_t bytes) __attribute__((nonnull));

int htfh_free(Allocator* alloc, void* ap) __attribute__((nonnull));
__attribute__((malloc
#if __GNUC__ >= 10
, malloc (htfh_free, 2)
#endif
)) void* htfh_malloc(Allocator* alloc, size_t nbytes) __attribute__((nonnull));
__attribute__((malloc
#if __GNUC__ >= 10
, malloc (htfh_free, 2)
#endif
)) __attribute__((alloc_size(2,3))) void* htfh_calloc(Allocator* alloc, size_t count, size_t nbytes) __attribute__((nonnull));
__attribute__((malloc
#if __GNUC__ >= 10
, malloc (htfh_free, 2)
#endif
)) __attribute__((alloc_size(3))) void* htfh_realloc(Allocator* alloc, void* ap, size_t nbytes) __attribute__((nonnull(1)));

#ifdef __cplusplus
};
#endif

#endif // _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_