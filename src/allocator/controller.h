#pragma once

#ifndef _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_CONTROLLER_
#define _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_CONTROLLER_

#ifdef __cplusplus
extern "C" {
#endif

#include "block.h"

typedef struct Controller {
    BlockHeader block_null;
    unsigned int fl_bitmap;
    unsigned int sl_bitmap[FL_INDEX_COUNT];
    BlockHeader* blocks[FL_INDEX_COUNT][SL_INDEX_COUNT];
} Controller;

int controller_new(Controller* controller) __attribute__((nonnull));

BlockHeader* controller_find_free_block(Controller* controller, size_t size) __attribute__((nonnull));
void* controller_mark_block_used(Controller* controller, BlockHeader* block, size_t size) __attribute__((nonnull));
BlockHeader* controller_find_suitable_block(Controller* controller, int* fli, int* sli) __attribute__((nonnull));
int controller_remove_free_block(Controller* controller, BlockHeader* block, int fl, int sl);
int controller_trim_free_block(Controller* controller, BlockHeader* block, size_t size) __attribute__((nonnull));
void controller_block_insert(Controller* controller, BlockHeader* block);
int controller_insert_free_block(Controller* controller, BlockHeader* block, int fl, int sl);
void controller_insert_block(Controller* controller, BlockHeader* block) __attribute__((nonnull));

void mapping_insert(size_t size, int* fli, int* sli);
void mapping_search(size_t size, int* fli, int* sli);

#ifdef __cplusplus
};
#endif

#endif // _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_CONTROLLER_