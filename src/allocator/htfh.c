#include "htfh.h"
#include <stddef.h>
#include <sys/mman.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define __htfh_lock_lock_handled(lock) ({ \
    int lock_result = 0; \
    if (__htfh_lock_lock(lock) == EINVAL) { \
        set_alloc_errno_msg(MUTEX_LOCK_LOCK, strerror(EINVAL)); \
        lock_result = -1; \
    } \
    lock_result; \
})

#define __htfh_lock_unlock_handled(lock) ({ \
    int unlock_result = 0; \
    if ((unlock_result = __htfh_lock_unlock(lock)) != 0) { \
        set_alloc_errno_msg(MUTEX_LOCK_UNLOCK, strerror(unlock_result)); \
        unlock_result = -1; \
    } \
    unlock_result; \
})

/*
** Adjust an allocation size to be aligned to word size, and no smaller
** than internal minimum.
*/
static size_t adjust_request_size(size_t size, size_t align)
{
    size_t adjust = 0;
    if (size)
    {
        const size_t aligned = align_up(size, align);

        /* aligned sized must not exceed block_size_max or we'll go out of bounds on sl_bitmap */
        if (aligned < block_size_max)
        {
            adjust = htfh_max(aligned, block_size_min);
        }
    }
    return adjust;
}

/*
** Debugging utilities.
*/

typedef struct integrity_t
{
    int prev_status;
    int status;
} integrity_t;

#define htfh_insist(x) { htfh_assert(x); if (!(x)) { status--; } }

static void integrity_walker(void* ptr, size_t size, int used, void* user)
{
    BlockHeader* block = block_from_ptr(ptr);
    integrity_t* integ = htfh_cast(integrity_t*, user);
    const int this_prev_status = block_is_prev_free(block) ? 1 : 0;
    const int this_status = block_is_free(block) ? 1 : 0;
    const size_t this_block_size = block_size(block);

    int status = 0;
    (void)used;
    htfh_insist(integ->prev_status == this_prev_status && "prev status incorrect");
    htfh_insist(size == this_block_size && "block size incorrect");

    integ->prev_status = this_status;
    integ->status += status;
}

int htfh_check(htfh_t htfh)
{
    int i, j;

    Controller* control = htfh_cast(Controller*, htfh);
    int status = 0;

    /* Check that the free lists and bitmaps are accurate. */
    for (i = 0; i < FL_INDEX_COUNT; ++i)
    {
        for (j = 0; j < SL_INDEX_COUNT; ++j)
        {
            const int fl_map = control->fl_bitmap & (1U << i);
            const int sl_list = control->sl_bitmap[i];
            const int sl_map = sl_list & (1U << j);
            const BlockHeader* block = control->blocks[i][j];

            /* Check that first- and second-level lists agree. */
            if (!fl_map)
            {
                htfh_insist(!sl_map && "second-level map must be null");
            }

            if (!sl_map)
            {
                htfh_insist(block == &control->block_null && "block list must be null");
                continue;
            }

            /* Check that there is at least one free block. */
            htfh_insist(sl_list && "no free blocks in second-level map");
            htfh_insist(block != &control->block_null && "block should not be null");

            while (block != &control->block_null)
            {
                int fli, sli;
                htfh_insist(block_is_free(block) && "block should be free");
                htfh_insist(!block_is_prev_free(block) && "blocks should have coalesced");
                htfh_insist(!block_is_free(block_next(block)) && "blocks should have coalesced");
                htfh_insist(block_is_prev_free(block_next(block)) && "block should be free");
                htfh_insist(block_size(block) >= block_size_min && "block not minimum size");

                mapping_insert(block_size(block), &fli, &sli);
                htfh_insist(fli == i && sli == j && "block size indexed in wrong list");
                block = block->next_free;
            }
        }
    }

    return status;
}

#undef htfh_insist

static void default_walker(void* ptr, size_t size, int used, void* user)
{
    (void)user;
    printf("\t%p %s size: %x (%p)\n", ptr, used ? "used" : "free", (unsigned int)size, block_from_ptr(ptr));
}

void htfh_walk_pool(pool_t pool, htfh_walker walker, void* user)
{
    htfh_walker pool_walker = walker ? walker : default_walker;
    BlockHeader* block =
        offset_to_block(pool, -(int)block_header_overhead);

    while (block && !block_is_last(block))
    {
        pool_walker(
            block_to_ptr(block),
            block_size(block),
            !block_is_free(block),
            user);
        block = block_next(block);
    }
}

size_t htfh_block_size(void* ptr)
{
    size_t size = 0;
    if (ptr)
    {
        const BlockHeader* block = block_from_ptr(ptr);
        size = block_size(block);
    }
    return size;
}

int htfh_check_pool(pool_t pool)
{
    /* Check that the blocks are physically correct. */
    integrity_t integ = { 0, 0 };
    htfh_walk_pool(pool, integrity_walker, &integ);

    return integ.status;
}

/*
** Size of the TLSF structures in a given memory block passed to
** htfh_create, equal to the size of a Controller
*/
size_t htfh_size(void)
{
    return sizeof(Controller);
}

size_t htfh_align_size(void)
{
    return ALIGN_SIZE;
}

size_t htfh_block_size_min(void)
{
    return block_size_min;
}

size_t htfh_block_size_max(void)
{
    return block_size_max;
}

/*
** Overhead of the TLSF structures in a given memory block passed to
** htfh_add_pool, equal to the overhead of a free block and the
** sentinel block.
*/
size_t htfh_pool_overhead(void)
{
    return 2 * block_header_overhead;
}

size_t htfh_alloc_overhead(void)
{
    return block_header_overhead;
}

pool_t htfh_add_pool(htfh_t htfh, void* mem, size_t bytes)
{
    BlockHeader* block;
    BlockHeader* next;

    const size_t pool_overhead = htfh_pool_overhead();
    const size_t pool_bytes = align_down(bytes - pool_overhead, ALIGN_SIZE);

    if (((ptrdiff_t)mem % ALIGN_SIZE) != 0)
    {
        printf("htfh_add_pool: Memory must be aligned by %u bytes.\n",
               (unsigned int)ALIGN_SIZE);
        return 0;
    }

    if (pool_bytes < block_size_min || pool_bytes > block_size_max)
    {
#if defined (TLSF_64BIT)
        printf("htfh_add_pool: Memory size must be between 0x%x and 0x%x00 bytes.\n",
               (unsigned int)(pool_overhead + block_size_min),
               (unsigned int)((pool_overhead + block_size_max) / 256));
#else
        printf("htfh_add_pool: Memory size must be between %u and %u bytes.\n",
			(unsigned int)(pool_overhead + block_size_min),
			(unsigned int)(pool_overhead + block_size_max));
#endif
        return 0;
    }

    /*
    ** Create the main free block. Offset the start of the block slightly
    ** so that the prev_phys_block field falls outside of the pool -
    ** it will never be used.
    */
    block = offset_to_block(mem, -(ptrdiff_t)block_header_overhead);
    block_set_size(block, pool_bytes);
    block_set_free(block);
    block_set_prev_used(block);
    controller_block_insert(htfh_cast(Controller*, htfh), block);

    /* Split the block to create a zero-size sentinel block. */
    next = block_link_next(block);
    block_set_size(next, 0);
    block_set_used(next);
    block_set_prev_free(next);

    return mem;
}

void htfh_remove_pool(htfh_t htfh, pool_t pool)
{
    Controller* control = htfh_cast(Controller*, htfh);
    BlockHeader* block = offset_to_block(pool, -(int)block_header_overhead);

    int fl = 0, sl = 0;

        htfh_assert(block_is_free(block) && "block should be free");
        htfh_assert(!block_is_free(block_next(block)) && "next block should not be free");
        htfh_assert(block_size(block_next(block)) == 0 && "next block size should be zero");

    mapping_insert(block_size(block), &fl, &sl);
    controller_remove_free_block(control, block, fl, sl);
}

/*
** TLSF main interface.
*/

#if _DEBUG
int test_ffs_fls()
{
	/* Verify ffs/fls work properly. */
	int rv = 0;
	rv += (htfh_ffs(0) == -1) ? 0 : 0x1;
	rv += (htfh_fls(0) == -1) ? 0 : 0x2;
	rv += (htfh_ffs(1) == 0) ? 0 : 0x4;
	rv += (htfh_fls(1) == 0) ? 0 : 0x8;
	rv += (htfh_ffs(0x80000000) == 31) ? 0 : 0x10;
	rv += (htfh_ffs(0x80008000) == 15) ? 0 : 0x20;
	rv += (htfh_fls(0x80000008) == 31) ? 0 : 0x40;
	rv += (htfh_fls(0x7FFFFFFF) == 30) ? 0 : 0x80;

#if defined (TLSF_64BIT)
	rv += (htfh_fls_sizet(0x80000000) == 31) ? 0 : 0x100;
	rv += (htfh_fls_sizet(0x100000000) == 32) ? 0 : 0x200;
	rv += (htfh_fls_sizet(0xffffffffffffffff) == 63) ? 0 : 0x400;
#endif

	if (rv)
	{
		printf("test_ffs_fls: %x ffs/fls tests failed.\n", rv);
	}
	return rv;
}
#endif

htfh_t htfh_create(void* mem)
{
#if _DEBUG
    if (test_ffs_fls())
	{
		return 0;
	}
#endif

    if (((ptrdiff_t)mem % ALIGN_SIZE) != 0)
    {
        printf("htfh_create: Memory must be aligned to %u bytes.\n",
               (unsigned int)ALIGN_SIZE);
        return 0;
    }

    controller_construct(htfh_cast(Controller*, mem));

    return htfh_cast(htfh_t, mem);
}

htfh_t htfh_create_with_pool(void* mem, size_t bytes)
{
    htfh_t htfh = htfh_create(mem);
    htfh_add_pool(htfh, (char*)mem + htfh_size(), bytes - htfh_size());
    return htfh;
}

void htfh_destroy(htfh_t htfh)
{
    /* Nothing to do. */
    (void)htfh;
}

pool_t htfh_get_pool(htfh_t htfh)
{
    return htfh_cast(pool_t, (char*)htfh + htfh_size());
}

void* htfh_malloc(htfh_t htfh, size_t size)
{
    Controller* control = htfh_cast(Controller*, htfh);
    const size_t adjust = adjust_request_size(size, ALIGN_SIZE);
    BlockHeader* block = controller_block_locate_free(control, adjust);
    return controller_block_prepare_used(control, block, adjust);
}

void* htfh_memalign(htfh_t htfh, size_t align, size_t size)
{
    Controller* control = htfh_cast(Controller*, htfh);
    const size_t adjust = adjust_request_size(size, ALIGN_SIZE);

    /*
    ** We must allocate an additional minimum block size bytes so that if
    ** our free block will leave an alignment gap which is smaller, we can
    ** trim a leading free block and release it back to the pool. We must
    ** do this because the previous physical block is in use, therefore
    ** the prev_phys_block field is not valid, and we can't simply adjust
    ** the size of that block.
    */
    const size_t gap_minimum = sizeof(BlockHeader);
    const size_t size_with_gap = adjust_request_size(adjust + align + gap_minimum, align);

    /*
    ** If alignment is less than or equals base alignment, we're done.
    ** If we requested 0 bytes, return null, as htfh_malloc(0) does.
    */
    const size_t aligned_size = (adjust && align > ALIGN_SIZE) ? size_with_gap : adjust;

    BlockHeader* block = controller_block_locate_free(control, aligned_size);

    /* This can't be a static assert. */
        htfh_assert(sizeof(BlockHeader) == block_size_min + block_header_overhead);

    if (block)
    {
        void* ptr = block_to_ptr(block);
        void* aligned = align_ptr(ptr, align);
        size_t gap = htfh_cast(size_t,
                               htfh_cast(ptrdiff_t, aligned) - htfh_cast(ptrdiff_t, ptr));

        /* If gap size is too small, offset to next aligned boundary. */
        if (gap && gap < gap_minimum)
        {
            const size_t gap_remain = gap_minimum - gap;
            const size_t offset = htfh_max(gap_remain, align);
            const void* next_aligned = htfh_cast(void*,
                                                 htfh_cast(ptrdiff_t, aligned) + offset);

            aligned = align_ptr(next_aligned, align);
            gap = htfh_cast(size_t,
                            htfh_cast(ptrdiff_t, aligned) - htfh_cast(ptrdiff_t, ptr));
        }

        if (gap)
        {
                htfh_assert(gap >= gap_minimum && "gap size too small");
            block = controller_block_trim_free_leading(control, block, gap);
        }
    }

    return controller_block_prepare_used(control, block, adjust);
}

void htfh_free(htfh_t htfh, void* ptr)
{
    /* Don't attempt to free a NULL pointer. */
    if (ptr)
    {
        Controller* control = htfh_cast(Controller*, htfh);
        BlockHeader* block = block_from_ptr(ptr);
            htfh_assert(!block_is_free(block) && "block already marked as free");
        block_mark_as_free(block);
        block = controller_block_merge_prev(control, block);
        block = controller_block_merge_next(control, block);
        controller_block_insert(control, block);
    }
}

/*
** The TLSF block information provides us with enough information to
** provide a reasonably intelligent implementation of realloc, growing or
** shrinking the currently allocated block as required.
**
** This routine handles the somewhat esoteric edge cases of realloc:
** - a non-zero size with a null pointer will behave like malloc
** - a zero size with a non-null pointer will behave like free
** - a request that cannot be satisfied will leave the original buffer
**   untouched
** - an extended buffer size will leave the newly-allocated area with
**   contents undefined
*/
void* htfh_realloc(htfh_t htfh, void* ptr, size_t size)
{
    Controller* control = htfh_cast(Controller*, htfh);
    void* p = 0;

    /* Zero-size requests are treated as free. */
    if (ptr && size == 0)
    {
        htfh_free(htfh, ptr);
    }
        /* Requests with NULL pointers are treated as malloc. */
    else if (!ptr)
    {
        p = htfh_malloc(htfh, size);
    }
    else
    {
        BlockHeader* block = block_from_ptr(ptr);
        BlockHeader* next = block_next(block);

        const size_t cursize = block_size(block);
        const size_t combined = cursize + block_size(next) + block_header_overhead;
        const size_t adjust = adjust_request_size(size, ALIGN_SIZE);

            htfh_assert(!block_is_free(block) && "block already marked as free");

        /*
        ** If the next block is used, or when combined with the current
        ** block, does not offer enough space, we must reallocate and copy.
        */
        if (adjust > cursize && (!block_is_free(next) || adjust > combined))
        {
            p = htfh_malloc(htfh, size);
            if (p)
            {
                const size_t minsize = htfh_min(cursize, size);
                memcpy(p, ptr, minsize);
                htfh_free(htfh, ptr);
            }
        }
        else
        {
            /* Do we need to expand to the next block? */
            if (adjust > cursize)
            {
                controller_block_merge_next(control, block);
                block_mark_as_used(block);
            }

            /* Trim the resulting block and return the original pointer. */
            controller_block_trim_used(control, block, adjust);
            p = ptr;
        }
    }

    return p;
}