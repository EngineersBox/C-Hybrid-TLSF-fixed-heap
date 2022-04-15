#include "block.h"

size_t block_size(const BlockHeader* block)
{
    return block->size & ~(block_header_free_bit | block_header_prev_free_bit);
}

void block_set_size(BlockHeader* block, size_t size)
{
    const size_t oldsize = block->size;
    block->size = size | (oldsize & (block_header_free_bit | block_header_prev_free_bit));
}

int block_is_last(const BlockHeader* block)
{
    return block_size(block) == 0;
}

int block_is_free(const BlockHeader* block)
{
    return htfh_cast(int, block->size & block_header_free_bit);
}

void block_set_free(BlockHeader* block)
{
    block->size |= block_header_free_bit;
}

void block_set_used(BlockHeader* block)
{
    block->size &= ~block_header_free_bit;
}

int block_is_prev_free(const BlockHeader* block)
{
    return htfh_cast(int, block->size & block_header_prev_free_bit);
}

void block_set_prev_free(BlockHeader* block)
{
    block->size |= block_header_prev_free_bit;
}

void block_set_prev_used(BlockHeader* block)
{
    block->size &= ~block_header_prev_free_bit;
}

BlockHeader* block_from_ptr(const void* ptr)
{
    return htfh_cast(BlockHeader*,
                     htfh_cast(unsigned char*, ptr) - block_start_offset);
}

void* block_to_ptr(const BlockHeader* block)
{
    return htfh_cast(void*,
                     htfh_cast(unsigned char*, block) + block_start_offset);
}

/* Return location of next block after block of given size. */
BlockHeader* offset_to_block(const void* ptr, size_t size)
{
    return htfh_cast(BlockHeader*, htfh_cast(ptrdiff_t, ptr) + size);
}

/* Return location of previous block. */
BlockHeader* block_prev(const BlockHeader* block)
{
        htfh_assert(block_is_prev_free(block) && "previous block must be free");
    return block->prev_phys_block;
}

/* Return location of next existing block. */
BlockHeader* block_next(const BlockHeader* block)
{
    BlockHeader* next = offset_to_block(block_to_ptr(block),
                                           block_size(block) - block_header_overhead);
        htfh_assert(!block_is_last(block));
    return next;
}

/* Link a new block with its physical neighbor, return the neighbor. */
BlockHeader* block_link_next(BlockHeader* block)
{
    BlockHeader* next = block_next(block);
    next->prev_phys_block = block;
    return next;
}

void block_mark_as_free(BlockHeader* block)
{
    /* Link the block to the next block, first. */
    BlockHeader* next = block_link_next(block);
    block_set_prev_free(next);
    block_set_free(block);
}

void block_mark_as_used(BlockHeader* block)
{
    BlockHeader* next = block_next(block);
    block_set_prev_used(next);
    block_set_used(block);
}

int block_can_split(BlockHeader* block, size_t size)
{
    return block_size(block) >= sizeof(BlockHeader) + size;
}

/* Split a block into two, the second of which is free. */
BlockHeader* block_split(BlockHeader* block, size_t size)
{
    /* Calculate the amount of space left in the remaining block. */
    BlockHeader* remaining =
        offset_to_block(block_to_ptr(block), size - block_header_overhead);

    const size_t remain_size = block_size(block) - (size + block_header_overhead);

        htfh_assert(block_to_ptr(remaining) == align_ptr(block_to_ptr(remaining), ALIGN_SIZE)
                    && "remaining block not aligned properly");

        htfh_assert(block_size(block) == remain_size + size + block_header_overhead);
    block_set_size(remaining, remain_size);
        htfh_assert(block_size(remaining) >= block_size_min && "block split with invalid size");

    block_set_size(block, size);
    block_mark_as_free(remaining);

    return remaining;
}

/* Absorb a free block's storage into an adjacent previous free block. */
BlockHeader* block_absorb(BlockHeader* prev, BlockHeader* block)
{
        htfh_assert(!block_is_last(prev) && "previous block can't be last");
    /* Note: Leaves flags untouched. */
    prev->size += block_size(block) + block_header_overhead;
    block_link_next(prev);
    return prev;
}