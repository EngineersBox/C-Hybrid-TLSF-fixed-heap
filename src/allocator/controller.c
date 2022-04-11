#include "controller.h"
#include "../error/allocator_errno.h"

int controller_new(Controller* controller) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return -1;
    }
    controller->block_null.next_free = &controller->block_null;
    controller->block_null.prev_free = &controller->block_null;
    controller->fl_bitmap = 0;
    for (int i = 0; i < FL_INDEX_COUNT; i++) {
        controller->sl_bitmap[i] = 0;
        for (int j = 0; j < SL_INDEX_COUNT; j++) {
            controller->blocks[i][j] = &controller->block_null;
        }
    }
    return 0;
}

#ifdef __cplusplus
#define declaration inline
#else
#define declaration static
#endif

declaration int htfh_fls_generic(unsigned int word) {
    int bit = 32;
    if (!word) bit -= 1;
    if (!(word & 0xffff0000)) { word <<= 16; bit -= 16; }
    if (!(word & 0xff000000)) { word <<= 8; bit -= 8; }
    if (!(word & 0xf0000000)) { word <<= 4; bit -= 4; }
    if (!(word & 0xc0000000)) { word <<= 2; bit -= 2; }
    if (!(word & 0x80000000)) { word <<= 1; bit -= 1; }
    return bit;
}

declaration int htfh_ffs(unsigned int word) {
    return htfh_fls_generic(word & (!word + 1)) - 1;
}

declaration int htfh_fls(unsigned int word) {
    return htfh_fls_generic(word) - 1;
}

#ifdef ARCH_64_BIT
declaration int tlsf_fls_sizet(size_t size) {
    int high = (int)(size >> 32);
    int bits = high ? 32 + htfh_fls(high) : htfh_fls((int) size & 0xffffffff);
    return bits;
}
#else
#define htfh_fls_sizet htfh_fls
#endif

#undef declaration

static void mapping_insert(size_t size, int* fli, int* sli) {
    int fl, sl;
    if (size < SMALL_BLOCK_SIZE) {
        /* Store small blocks in first list. */
        fl = 0;
        sl = (int) size / (SMALL_BLOCK_SIZE / SL_INDEX_COUNT);
    } else {
        fl = htfh_fls_sizet(size);
        sl = (int) (size >> (fl - SL_INDEX_COUNT_LOG2)) ^ (1 << SL_INDEX_COUNT_LOG2);
        fl -= (FL_INDEX_SHIFT - 1);
    }
    *fli = fl;
    *sli = sl;
}

static void mapping_search(size_t size, int* fli, int* sli) {
    if (size >= SMALL_BLOCK_SIZE) {
        const size_t round = (1 << (htfh_fls_sizet(size) - SL_INDEX_COUNT_LOG2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

static BlockHeader* controller_find_suitable_block(Controller* controller, int* fli, int* sli) {
    int fl = *fli;
    int sl = *sli;

    unsigned int sl_map = controller->sl_bitmap[fl] & (~0U << sl);
    if (!sl_map) {
        const unsigned int fl_map = controller->fl_bitmap & (~0U << (fl + 1));
        if (!fl_map) {
            return 0;
        }
        fl = htfh_ffs(fl_map);
        *fli = fl;
        sl_map = controller->sl_bitmap[fl];
    }
    if (!sl_map) {
        set_alloc_errno(SECOND_LEVEL_BITMAP_NULL);
        return NULL;
    }
    sl = htfh_ffs(sl_map);
    *sli = sl;
    return controller->blocks[fl][sl];
}

static int controller_remove_free_block(Controller* controller, BlockHeader* block, int fl, int sl) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return -1;
    } else if (block == NULL) {
        set_alloc_errno(BLOCK_IS_NULL);
        return -1;
    }
    BlockHeader* prev = block->prev_free;
    BlockHeader* next = block->next_free;
    if (prev == NULL) {
        set_alloc_errno(PREV_BLOCK_NULL);
        return -1;
    } else if (next == NULL) {
        set_alloc_errno(NEXT_BLOCK_NULL);
        return -1;
    }
    next->prev_free = prev;
    prev->next_free = next;
    if (controller->blocks[fl][sl] != block) {
        return 0;
    }
    controller->blocks[fl][sl] = next;
    if (next != &controller->block_null) {
        return 0;
    }
    controller->sl_bitmap[fl] &= ~(1U << sl);
    if (controller->sl_bitmap[fl]) {
        return 0;
    }
    controller->fl_bitmap &= ~(1U << fl);
    return 0;
}

static BlockHeader* controller_find_free_block(Controller* controller, size_t size) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return NULL;
    }
    int fl = 0;
    int sl = 0;
    BlockHeader* block = NULL;
    if (size) {
        mapping_search(size, &fl, &sl);
        if (sl < FL_INDEX_COUNT) {
            block = controller_find_suitable_block(controller, &fl, &sl);
        }
    }
    if (block) {
        if (block_size(block) >= size) {
            return NULL;
        }
        if (controller_remove_free_block(controller, block, fl, sl) != 0) {
            return NULL;
        }
    }
    return block;
}

static void controller_trim_free_block(Controller* controller, BlockHeader* block, size_t size) {
    
}

static void* controller_mark_block_used(Controller* controller, BlockHeader* block, size_t size) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return NULL;
    } else if (block == NULL) {
        set_alloc_errno(BLOCK_IS_NULL);
        return NULL;
    } else if (!size) {
        set_alloc_errno(NON_ZERO_BLOCK_SIZE);
        return NULL;
    }
    void* p = NULL;
    p = block_to_ptr(block);
    block_mark_as_used(block);
    return p;
}

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