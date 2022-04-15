#include "htfh.h"
#include <stddef.h>
#include <sys/mman.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define __htfh_lock_lock_handled(lock) ({ \
    int _lock_result = 0; \
    if (__htfh_lock_lock(lock) == EINVAL) { \
        set_alloc_errno_msg(MUTEX_LOCK_LOCK, strerror(EINVAL)); \
        _lock_result = -1; \
    } \
    _lock_result; \
})

#define __htfh_lock_unlock_handled(lock) ({ \
    int _unlock_result = 0; \
    if ((_unlock_result = __htfh_lock_unlock(lock)) != 0) { \
        set_alloc_errno_msg(MUTEX_LOCK_UNLOCK, strerror(_unlock_result)); \
        _unlock_result = -1; \
    } \
    _unlock_result; \
})

static size_t adjust_request_size(size_t size, size_t align) {
    size_t adjust = 0;
    if (!size) {
        return 0;
    }
    const size_t aligned = align_up(size, align);
    if (aligned < block_size_max) {
        adjust = htfh_max(aligned, block_size_min);
    }
    return adjust;
}

#define htfh_insist(x) { htfh_assert(x); if (!(x)) { status--; } }

static void integrity_walker(void* ptr, size_t size, int used, void* user) {
    BlockHeader* block = block_from_ptr(ptr);
    integrity_t* integ = user;
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

int htfh_check(Allocator* htfh) {
    int i, j;

    int status = 0;

    /* Check that the free lists and bitmaps are accurate. */
    for (i = 0; i < FL_INDEX_COUNT; ++i) {
        for (j = 0; j < SL_INDEX_COUNT; ++j) {
            const int fl_map = htfh->controller->fl_bitmap & (1U << i);
            const int sl_list = htfh->controller->sl_bitmap[i];
            const int sl_map = sl_list & (1U << j);
            const BlockHeader* block = htfh->controller->blocks[i][j];

            /* Check that first- and second-level lists agree. */
            if (!fl_map) {
                htfh_insist(!sl_map && "second-level map must be null");
            }

            if (!sl_map) {
                htfh_insist(block == &htfh->controller->block_null && "block list must be null");
                continue;
            }

            /* Check that there is at least one free block. */
            htfh_insist(sl_list && "no free blocks in second-level map");
            htfh_insist(block != &htfh->controller->block_null && "block should not be null");

            while (block != &htfh->controller->block_null) {
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

static void default_walker(void* ptr, size_t size, int used, void* user) {
    (void)user;
    printf("\t%p %s size: %x (%p)\n", ptr, used ? "used" : "free", (unsigned int)size, block_from_ptr(ptr));
}

void htfh_walk_pool(pool_t pool, htfh_walker walker, void* user) {
    htfh_walker pool_walker = walker ? walker : default_walker;
    BlockHeader* block = offset_to_block(pool, -(int)block_header_overhead);

    while (block && !block_is_last(block)) {
        pool_walker(
            block_to_ptr(block),
            block_size(block),
            !block_is_free(block),
            user);
        block = block_next(block);
    }
}

size_t htfh_block_size(void* ptr) {
    size_t size = 0;
    if (ptr) {
        const BlockHeader* block = block_from_ptr(ptr);
        size = block_size(block);
    }
    return size;
}

int htfh_check_pool(pool_t pool) {
    /* Check that the blocks are physically correct. */
    integrity_t integ = { 0, 0 };
    htfh_walk_pool(pool, integrity_walker, &integ);
    return integ.status;
}

size_t htfh_size(void) {
    return sizeof(Controller);
}

size_t htfh_align_size(void) {
    return ALIGN_SIZE;
}

size_t htfh_block_size_min(void) {
    return block_size_min;
}

size_t htfh_block_size_max(void) {
    return block_size_max;
}

size_t htfh_pool_overhead(void) {
    return 2 * block_header_overhead;
}

size_t htfh_alloc_overhead(void) {
    return block_header_overhead;
}

pool_t htfh_add_pool(Allocator* alloc, void* mem, size_t bytes) {
    if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return NULL;
    }
    BlockHeader* block;
    BlockHeader* next;
    const size_t pool_overhead = htfh_pool_overhead();
    const size_t pool_bytes = align_down(bytes - pool_overhead, ALIGN_SIZE);
    if (((ptrdiff_t) mem % ALIGN_SIZE) != 0) {
        set_alloc_errno(POOL_MISALIGNED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return NULL;
    }
    if (pool_bytes < block_size_min || pool_bytes > block_size_max) {
        char msg[100];
        sprintf(
            msg,
            "Memory pool must be between 0x%x and 0x%x00 bytes: ",
            #ifdef ARCH_64_BIT
            (unsigned int)(pool_overhead + block_size_min),
            (unsigned int)((pool_overhead + block_size_max) / 256)
#else
            (unsigned int)(pool_overhead + block_size_min),
            (unsigned int)(pool_overhead + block_size_max)
#endif
        );
        set_alloc_errno_msg(INVALID_POOL_SIZE, msg);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return NULL;
    }
    block = offset_to_block(mem, -(ptrdiff_t)block_header_overhead);
    block_set_size(block, pool_bytes);
    block_set_free(block);
    block_set_prev_used(block);
    controller_block_insert(alloc->controller, block);

    next = block_link_next(block);
    block_set_size(next, 0);
    block_set_used(next);
    block_set_prev_free(next);

    if (__htfh_lock_unlock_handled(&alloc->mutex) != 0) {
        return NULL;
    }
    return mem;
}

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

#if defined (ARCH_64_BIT)
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

Allocator* htfh_create(size_t bytes) {
#if _DEBUG
    if (test_ffs_fls())
	{
		return 0;
	}
#endif
    if ((bytes % ALIGN_SIZE) != 0) {
        char msg[100];
        sprintf(msg, "Memory must be aligned to %u bytes", (unsigned int) ALIGN_SIZE);
        set_alloc_errno_msg(HEAP_MISALIGNED, msg);
        return NULL;
    }
    Allocator* alloc = malloc(sizeof(*alloc));
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    }
    int lock_result;
    if ((lock_result = __htfh_lock_init(&alloc->mutex, PTHREAD_MUTEX_RECURSIVE)) != 0) {
        set_alloc_errno_msg(MUTEX_LOCK_INIT, strerror(lock_result));
        return NULL;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) != 0) {
        return NULL;
    }
    alloc->heap_size = bytes;
    alloc->controller = alloc->heap = mmap(
        NULL,
        bytes,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (alloc->heap == NULL) {
        set_alloc_errno(HEAP_MMAP_FAILED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return NULL;
    }
    controller_construct(alloc->controller);
    htfh_add_pool(alloc, (char*)alloc->heap + htfh_size(), bytes - htfh_size());
    return __htfh_lock_unlock_handled(&alloc->mutex) == 0 ? alloc : NULL;
}

int htfh_destroy(Allocator* alloc) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    } else if (alloc->controller == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) != 0) {
        return -1;
    }
    if (munmap(alloc->heap, alloc->heap_size) != 0 ) {
        set_alloc_errno(HEAP_UNMAP_FAILED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    if (__htfh_lock_unlock_handled(&alloc->mutex) != 0) {
        return -1;
    }
    free(alloc);
    return 0;
}

void* htfh_malloc(Allocator* alloc, size_t size) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (alloc->controller == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return NULL;
    }
    const size_t adjust = adjust_request_size(size, ALIGN_SIZE);
    BlockHeader* block = controller_block_locate_free(alloc->controller, adjust);
    void* ptr = controller_block_prepare_used(alloc->controller, block, adjust);
    return __htfh_lock_unlock_handled(&alloc->mutex) == 0 ? ptr : NULL;
}

int htfh_free(Allocator* alloc, void* ptr) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    } else if (alloc->controller == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return -1;
    }
    /* Don't attempt to free a NULL pointer. */
    if (ptr == NULL) {
        return __htfh_lock_unlock_handled(&alloc->mutex);
    }
    BlockHeader* block = block_from_ptr(ptr);
    if (block == NULL) {
        set_alloc_errno(BLOCK_IS_NULL);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    } else if (block_is_free(block)) {
        set_alloc_errno(BLOCK_ALREADY_FREED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    block_mark_as_free(block);
    block = controller_block_merge_prev(alloc->controller, block);
    block = controller_block_merge_next(alloc->controller, block);
    controller_block_insert(alloc->controller, block);
    return __htfh_lock_unlock_handled(&alloc->mutex);
}

void* htfh_memalign(Allocator* alloc, size_t align, size_t size) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (alloc->controller == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return NULL;
    }
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

    BlockHeader* block = controller_block_locate_free(alloc->controller, aligned_size);

    /* This can't be a static assert. */
        htfh_assert(sizeof(BlockHeader) == block_size_min + block_header_overhead);

    if (block) {
        void* ptr = block_to_ptr(block);
        void* aligned = align_ptr(ptr, align);
        size_t gap = htfh_cast(size_t,
                               htfh_cast(ptrdiff_t, aligned) - htfh_cast(ptrdiff_t, ptr));

        /* If gap size is too small, offset to next aligned boundary. */
        if (gap && gap < gap_minimum) {
            const size_t gap_remain = gap_minimum - gap;
            const size_t offset = htfh_max(gap_remain, align);
            const void* next_aligned = htfh_cast(void*,
                                                 htfh_cast(ptrdiff_t, aligned) + offset);

            aligned = align_ptr(next_aligned, align);
            gap = htfh_cast(size_t,
                            htfh_cast(ptrdiff_t, aligned) - htfh_cast(ptrdiff_t, ptr));
        }

        if (gap) {
                htfh_assert(gap >= gap_minimum && "gap size too small");
            block = controller_block_trim_free_leading(alloc->controller, block, gap);
        }
    }

    void* ptr = controller_block_prepare_used(alloc->controller, block, adjust);
    return __htfh_lock_unlock_handled(&alloc->mutex) == 0 ? ptr : NULL;
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
void* htfh_realloc(Allocator* alloc, void* ptr, size_t size) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (alloc->controller == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return NULL;
    }
    void* p = 0;
    /* Zero-size requests are treated as free. */
    if (ptr && size == 0) {
        htfh_free(alloc, ptr);
    }
        /* Requests with NULL pointers are treated as malloc. */
    else if (!ptr) {
        p = htfh_malloc(alloc, size);
    } else {
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
        if (adjust > cursize && (!block_is_free(next) || adjust > combined)) {
            p = htfh_malloc(alloc, size);
            if (p) {
                const size_t minsize = htfh_min(cursize, size);
                memcpy(p, ptr, minsize);
                htfh_free(alloc, ptr);
            }
        } else {
            /* Do we need to expand to the next block? */
            if (adjust > cursize) {
                controller_block_merge_next(alloc->controller, block);
                block_mark_as_used(block);
            }

            /* Trim the resulting block and return the original pointer. */
            controller_block_trim_used(alloc->controller, block, adjust);
            p = ptr;
        }
    }

    return __htfh_lock_unlock_handled(&alloc->mutex) == 0 ? p : NULL;
}