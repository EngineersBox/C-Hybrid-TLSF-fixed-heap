#include "controller.h"

BlockHeader* controller_search_suitable_block(Controller* control, int* fli, int* sli)
{
    int fl = *fli;
    int sl = *sli;

    /*
    ** First, search for a block in the list associated with the given
    ** fl/sl index.
    */
    unsigned int sl_map = control->sl_bitmap[fl] & (~0U << sl);
    if (!sl_map)
    {
        /* No block exists. Search in the next largest first-level list. */
        const unsigned int fl_map = control->fl_bitmap & (~0U << (fl + 1));
        if (!fl_map)
        {
            /* No free blocks available, memory has been exhausted. */
            return 0;
        }

        fl = htfh_ffs(fl_map);
        *fli = fl;
        sl_map = control->sl_bitmap[fl];
    }
        htfh_assert(sl_map && "internal error - second level bitmap is null");
    sl = htfh_ffs(sl_map);
    *sli = sl;

    /* Return the first block in the free list. */
    return control->blocks[fl][sl];
}

/* Remove a free block from the free list.*/
void controller_remove_free_block(Controller* control, BlockHeader* block, int fl, int sl)
{
    BlockHeader* prev = block->prev_free;
    BlockHeader* next = block->next_free;
        htfh_assert(prev && "prev_free field can not be null");
        htfh_assert(next && "next_free field can not be null");
    next->prev_free = prev;
    prev->next_free = next;

    /* If this block is the head of the free list, set new head. */
    if (control->blocks[fl][sl] == block)
    {
        control->blocks[fl][sl] = next;

        /* If the new head is null, clear the bitmap. */
        if (next == &control->block_null)
        {
            control->sl_bitmap[fl] &= ~(1U << sl);

            /* If the second bitmap is now empty, clear the fl bitmap. */
            if (!control->sl_bitmap[fl])
            {
                control->fl_bitmap &= ~(1U << fl);
            }
        }
    }
}

/* Insert a free block into the free block list. */
void controller_insert_free_block(Controller* control, BlockHeader* block, int fl, int sl)
{
    BlockHeader* current = control->blocks[fl][sl];
        htfh_assert(current && "free list cannot have a null entry");
        htfh_assert(block && "cannot insert a null entry into the free list");
    block->next_free = current;
    block->prev_free = &control->block_null;
    current->prev_free = block;

        htfh_assert(block_to_ptr(block) == align_ptr(block_to_ptr(block), ALIGN_SIZE)
                    && "block not aligned properly");
    /*
    ** Insert the new block at the head of the list, and mark the first-
    ** and second-level bitmaps appropriately.
    */
    control->blocks[fl][sl] = block;
    control->fl_bitmap |= (1U << fl);
    control->sl_bitmap[fl] |= (1U << sl);
}

/* Remove a given block from the free list. */
void controller_block_remove(Controller* control, BlockHeader* block)
{
    int fl, sl;
    mapping_insert(block_size(block), &fl, &sl);
    controller_remove_free_block(control, block, fl, sl);
}

/* Insert a given block into the free list. */
void controller_block_insert(Controller* control, BlockHeader* block)
{
    int fl, sl;
    mapping_insert(block_size(block), &fl, &sl);
    controller_insert_free_block(control, block, fl, sl);
}

/* Merge a just-freed block with an adjacent previous free block. */
BlockHeader* controller_block_merge_prev(Controller* control, BlockHeader* block)
{
    if (block_is_prev_free(block))
    {
        BlockHeader* prev = block_prev(block);
            htfh_assert(prev && "prev physical block can't be null");
            htfh_assert(block_is_free(prev) && "prev block is not free though marked as such");
        controller_block_remove(control, prev);
        block = block_absorb(prev, block);
    }

    return block;
}

/* Merge a just-freed block with an adjacent free block. */
BlockHeader* controller_block_merge_next(Controller* control, BlockHeader* block)
{
    BlockHeader* next = block_next(block);
        htfh_assert(next && "next physical block can't be null");

    if (block_is_free(next))
    {
            htfh_assert(!block_is_last(block) && "previous block can't be last");
        controller_block_remove(control, next);
        block = block_absorb(block, next);
    }

    return block;
}

/* Trim any trailing block space off the end of a block, return to pool. */
void controller_block_trim_free(Controller* control, BlockHeader* block, size_t size)
{
        htfh_assert(block_is_free(block) && "block must be free");
    if (block_can_split(block, size))
    {
        BlockHeader* remaining_block = block_split(block, size);
        block_link_next(block);
        block_set_prev_free(remaining_block);
        controller_block_insert(control, remaining_block);
    }
}

/* Trim any trailing block space off the end of a used block, return to pool. */
void controller_block_trim_used(Controller* control, BlockHeader* block, size_t size)
{
        htfh_assert(!block_is_free(block) && "block must be used");
    if (block_can_split(block, size))
    {
        /* If the next block is free, we must coalesce. */
        BlockHeader* remaining_block = block_split(block, size);
        block_set_prev_used(remaining_block);

        remaining_block = controller_block_merge_next(control, remaining_block);
        controller_block_insert(control, remaining_block);
    }
}

BlockHeader* controller_block_trim_free_leading(Controller* control, BlockHeader* block, size_t size)
{
    BlockHeader* remaining_block = block;
    if (block_can_split(block, size))
    {
        /* We want the 2nd block. */
        remaining_block = block_split(block, size - block_header_overhead);
        block_set_prev_free(remaining_block);

        block_link_next(block);
        controller_block_insert(control, block);
    }

    return remaining_block;
}

BlockHeader* controller_block_locate_free(Controller* control, size_t size)
{
    int fl = 0, sl = 0;
    BlockHeader* block = 0;

    if (size)
    {
        mapping_search(size, &fl, &sl);

        /*
        ** mapping_search can futz with the size, so for excessively large sizes it can sometimes wind up
        ** with indices that are off the end of the block array.
        ** So, we protect against that here, since this is the only callsite of mapping_search.
        ** Note that we don't need to check sl, since it comes from a modulo operation that guarantees it's always in range.
        */
        if (fl < FL_INDEX_COUNT)
        {
            block = controller_search_suitable_block(control, &fl, &sl);
        }
    }

    if (block)
    {
            htfh_assert(block_size(block) >= size);
        controller_remove_free_block(control, block, fl, sl);
    }

    return block;
}

void* controller_block_prepare_used(Controller* control, BlockHeader* block, size_t size)
{
    void* p = 0;
    if (block)
    {
            htfh_assert(size && "size must be non-zero");
        controller_block_trim_free(control, block, size);
        block_mark_as_used(block);
        p = block_to_ptr(block);
    }
    return p;
}

/* Clear structure and point all empty lists at the null block. */
void controller_construct(Controller* control)
{
    int i, j;

    control->block_null.next_free = &control->block_null;
    control->block_null.prev_free = &control->block_null;

    control->fl_bitmap = 0;
    for (i = 0; i < FL_INDEX_COUNT; ++i)
    {
        control->sl_bitmap[i] = 0;
        for (j = 0; j < SL_INDEX_COUNT; ++j)
        {
            control->blocks[i][j] = &control->block_null;
        }
    }
}