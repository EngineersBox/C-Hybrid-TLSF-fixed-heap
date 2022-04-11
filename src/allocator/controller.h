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

int controller_new(Controller* controller);

#ifdef __cplusplus
};
#endif

#endif // _C_HYBRID_TLSF_FIXED_HEAP_ALLOCATOR_CONTROLLER_