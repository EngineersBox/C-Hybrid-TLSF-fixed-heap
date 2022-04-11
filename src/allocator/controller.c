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
    printf("bit: %d\n", bit);
    return bit;
}

declaration int htfh_ffs(unsigned int word) {
    return htfh_fls_generic(word & (~word + 1)) - 1;
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

void mapping_insert(size_t size, int* fli, int* sli) {
    int fl, sl;
    if (size < SMALL_BLOCK_SIZE) {
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

void mapping_search(size_t size, int* fli, int* sli) {
    if (size >= SMALL_BLOCK_SIZE) {
        const size_t round = (1 << (htfh_fls_sizet(size) - SL_INDEX_COUNT_LOG2)) - 1;
        size += round;
    }
    mapping_insert(size, fli, sli);
}

BlockHeader* controller_find_suitable_block(Controller* controller, int* fli, int* sli) {
    int fl = *fli;
    int sl = *sli;

    unsigned int sl_map = controller->sl_bitmap[fl] & (~0U << sl);
    if (!sl_map) {
        const unsigned int fl_map = controller->fl_bitmap & (~0U << (fl + 1));
        if (!fl_map) {
            set_alloc_errno(HEAP_FULL);
            return NULL;
        }
        fl = htfh_ffs(fl_map);
        printf("FL: %d\n", fl);
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

int controller_remove_free_block(Controller* controller, BlockHeader* block, int fl, int sl) {
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

BlockHeader* controller_find_free_block(Controller* controller, size_t size) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return NULL;
    }
    int fl = 0;
    int sl = 0;
    BlockHeader* block = NULL;
    if (size) {
        printf("%zu", size);
        mapping_search(size, &fl, &sl);
        if (fl < FL_INDEX_COUNT) {
            block = controller_find_suitable_block(controller, &fl, &sl);
        }
        printf("%p", block);
    }
    if (block != NULL) {
        if (block_size(block) >= size) {
            return NULL;
        }
        if (controller_remove_free_block(controller, block, fl, sl) != 0) {
            return NULL;
        }
    }
    return block;
}

int controller_trim_free_block(Controller* controller, BlockHeader* block, size_t size) {
    if (!block_is_free(block)) {
        set_alloc_errno(BLOCK_NOT_FREE);
        return -1;
    }
    if (!block_can_split(block, size)) {
        return 0;
    }
    BlockHeader* remaining_block = block_split(block, size);
    if (remaining_block == NULL) {
        return -1;
    }
    block_link_next(block);
    block_set_prev_free(remaining_block);
    controller_block_insert(controller, remaining_block);
    return 0;
}

void* controller_mark_block_used(Controller* controller, BlockHeader* block, size_t size) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return NULL;
    }
    void* p = NULL;
    if (block) {
        if (!size) {
            set_alloc_errno(NON_ZERO_BLOCK_SIZE);
            return NULL;
        }
        controller_trim_free_block(controller, block, size);
        block_mark_as_used(block);
        p = block_to_ptr(block);
    }
    return p;
}

int controller_insert_free_block(Controller* controller, BlockHeader* block, int fl, int sl) {
    if (controller == NULL) {
        set_alloc_errno(NULL_CONTROLLER_INSTANCE);
        return -1;
    } else if (block == NULL) {
        set_alloc_errno(BLOCK_IS_NULL);
        return -1;
    }
    BlockHeader* current = controller->blocks[fl][sl];
    if (current == NULL) {
        set_alloc_errno_msg(BLOCK_IS_NULL, "Free list cannot have a null entry");
        return -1;
    }
    block->next_free = current;
    block->prev_free = &controller->block_null;
    current->prev_free = block;
    if (block_to_ptr(block) != align_ptr(block_to_ptr(block), ALIGN_SIZE)) {
        set_alloc_errno(BLOCK_NOT_ALIGNED);
        return -1;
    }
    controller->blocks[fl][sl] = block;
    controller->fl_bitmap |= (1U << fl);
    controller->sl_bitmap[fl] |= (1U << sl);
    return 0;
}

void controller_block_insert(Controller* controller, BlockHeader* block) {
    int fl;
    int sl;
    mapping_insert(block_size(block), &fl, &sl);
    controller_insert_free_block(controller, block, fl, sl);
}