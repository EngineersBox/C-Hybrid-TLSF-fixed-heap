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

/* htfh_t: a TLSF structure. Can contain 1 to N pools. */
/* pool_t: a block of memory that TLSF can manage. */
typedef void* htfh_t;
typedef void* pool_t;

/* Create/destroy a memory pool. */
htfh_t htfh_create(void* mem);
htfh_t htfh_create_with_pool(void* mem, size_t bytes);
void htfh_destroy(htfh_t htfh);
pool_t htfh_get_pool(htfh_t htfh);

/* Add/remove memory pools. */
pool_t htfh_add_pool(htfh_t htfh, void* mem, size_t bytes);
void htfh_remove_pool(htfh_t htfh, pool_t pool);

/* malloc/memalign/realloc/free replacements. */
void htfh_free(htfh_t htfh, void* ptr);
__attribute__((malloc
#if __GNUC__ >= 10
, malloc (htfh_free, 2)
#endif
)) __attribute__((alloc_size(2)))  void* htfh_malloc(htfh_t htfh, size_t bytes);
__attribute__((malloc
#if __GNUC__ >= 10
, malloc (htfh_free, 2)
#endif
)) __attribute__((alloc_size(2,3))) void* htfh_calloc(htfh_t htfh, size_t count, size_t nbytes);
__attribute__((malloc
#if __GNUC__ >= 10
, malloc (htfh_free, 2)
#endif
)) void* htfh_memalign(htfh_t htfh, size_t align, size_t bytes);
__attribute__((malloc
#if __GNUC__ >= 10
, malloc (htfh_free, 2)
#endif
)) __attribute__((alloc_size(3))) void* htfh_realloc(htfh_t htfh, void* ptr, size_t size);

/* Returns internal block size, not original request size */
size_t htfh_block_size(void* ptr);

/* Overheads/limits of internal structures. */
size_t htfh_size(void);
size_t htfh_align_size(void);
size_t htfh_block_size_min(void);
size_t htfh_block_size_max(void);
size_t htfh_pool_overhead(void);
size_t htfh_alloc_overhead(void);

/* Debugging. */
typedef void (*htfh_walker)(void* ptr, size_t size, int used, void* user);
void htfh_walk_pool(pool_t pool, htfh_walker walker, void* user);
/* Returns nonzero if any internal consistency check fails. */
int htfh_check(htfh_t htfh);
int htfh_check_pool(pool_t pool);

#ifdef __cplusplus
};
#endif

#endif // _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_