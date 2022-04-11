#pragma once

#ifndef _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_BLOCK_
#define _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_BLOCK_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stddef.h>

#include "constants.h"

typedef struct BlockHeader {
    struct BlockHeader* prev_physical_block;
    size_t size;
    struct BlockHeader* next_free;
    struct BlockHeader* prev_free;
} BlockHeader;

static const size_t block_header_free_bit = 1 << 0;
static const size_t block_header_prev_free_bit = 1 << 1;
static const size_t block_header_overhead = sizeof(size_t);
static const size_t block_start_offset = offsetof(BlockHeader, size) + sizeof(size_t);
static const size_t block_size_min = sizeof(BlockHeader) - sizeof(BlockHeader*);
static const size_t block_size_max = ((size_t) 1) << FL_INDEX_MAX;

static size_t block_size(const BlockHeader* block) __attribute__((nonnull(1)));
static void block_set_size(BlockHeader* block, size_t size) __attribute__((nonnull(1)));
static int block_is_last(const BlockHeader* block) __attribute__((nonnull(1)));
static int block_is_free(const BlockHeader* block) __attribute__((nonnull(1)));
static void block_set_free(BlockHeader* block) __attribute__((nonnull(1)));
static void block_set_used(BlockHeader* block) __attribute__((nonnull(1)));
static int block_is_prev_free(const BlockHeader* block) __attribute__((nonnull(1)));
static void block_set_prev_free(BlockHeader* block) __attribute__((nonnull(1)));
static void block_set_prev_used(BlockHeader* block) __attribute__((nonnull(1)));
static BlockHeader* block_from_ptr(const void* ptr) __attribute__((nonnull(1)));
static void* block_to_ptr(const BlockHeader* block) __attribute__((nonnull(1)));
static BlockHeader* offset_to_block(const void* ptr, size_t size) __attribute__((nonnull(1)));
static BlockHeader* block_prev(const BlockHeader* block) __attribute__((nonnull(1)));
static BlockHeader* block_next(const BlockHeader* block) __attribute__((nonnull(1)));
static BlockHeader* block_link_next(BlockHeader* block) __attribute__((nonnull(1)));
static void block_mark_as_free(BlockHeader* block) __attribute__((nonnull(1)));
static void block_mark_as_used(BlockHeader* block) __attribute__((nonnull(1)));

#ifdef __cplusplus
};
#endif

#endif // _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_BLOCK_