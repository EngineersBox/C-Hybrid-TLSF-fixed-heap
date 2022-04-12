#include "tlsf.h"
#include <stddef.h>
#include <sys/mman.h>

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

// ==== INTERNAL ====

static size_t fit_allocation_size(size_t size, size_t alignment) {
    size_t fit = 0;
    if (!size) {
        return fit;
    }
    const size_t aligned = align_up(size, alignment);
    if (aligned < block_size_max) {
        fit = aligned > block_size_min ? aligned : block_size_min;
    }
    return fit;
}

#define htfh_pool_overhead (2 * block_header_overhead)
#define htfh_alloc_overhead block_header_overhead

// ==== PUBLIC API ====

heap_mem_pool_t htfh_add_pool(Allocator* alloc, heap_mem_pool_t mem, size_t bytes) {
    if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return NULL;
    }
    BlockHeader* block;
    BlockHeader* next;
    const size_t pool_overhead = htfh_pool_overhead;
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

int htfh_new(Allocator* alloc) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    }
    int lock_result;
    if ((lock_result = __htfh_lock_init(&alloc->mutex, PTHREAD_MUTEX_RECURSIVE)) != 0) {
        set_alloc_errno_msg(MUTEX_LOCK_INIT, strerror(lock_result));
        return -1;
    }
    alloc->heap_size = 0;
    alloc->heap = NULL;
    alloc->controller = NULL;
    return 0;
}

int htfh_init(Allocator* alloc, size_t heap_size) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return -1;
    } else if (alloc->heap != NULL) {
        set_alloc_errno(HEAP_ALREADY_MAPPED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    } else if (heap_size % ALIGN_SIZE != 0) {
        set_alloc_errno(HEAP_MISALIGNED);
        return -1;
    }
    alloc->heap_size = heap_size;
    alloc->controller = alloc->heap = mmap(
        NULL,
        heap_size,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );
    if (alloc->heap == NULL) {
        set_alloc_errno(HEAP_MMAP_FAILED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    if (controller_new(alloc->controller) != 0) {
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    if (htfh_add_pool(alloc, alloc->heap, heap_size) == NULL) {
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    return __htfh_lock_unlock_handled(&alloc->mutex);
}

int htfh_destruct(Allocator* alloc) {
    if (alloc == NULL) {
        return 0;
    }
    if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return -1;
    }
    if (alloc->heap != NULL && munmap(alloc->heap, alloc->heap_size) != 0) {
        set_alloc_errno(HEAP_UNMAP_FAILED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    if (__htfh_lock_unlock_handled(&alloc->mutex) == -1) {
        return -1;
    }
    free(alloc);
    return 0;
}

void* htfh_malloc(Allocator* alloc, size_t nbytes) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (alloc->controller == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return NULL;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return NULL;
    }
    const size_t fitted_allocation_size = fit_allocation_size(nbytes, ALIGN_SIZE);
    BlockHeader* block = controller_find_free_block(alloc->controller, fitted_allocation_size);
    void* ptr = controller_mark_block_used(alloc->controller, block, fitted_allocation_size);
    __htfh_lock_unlock_handled(&alloc->mutex);
    return ptr;
}

int htfh_free(Allocator* alloc, void* ptr) {
    if (alloc == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    } else if (alloc->controller == NULL) {
        set_alloc_errno(NULL_ALLOCATOR_INSTANCE);
        return -1;
    } else if (ptr == NULL) {
        set_alloc_errno(FREE_NULL_PTR);
        return -1;
    } else if (__htfh_lock_lock_handled(&alloc->mutex) == -1) {
        return -1;
    }
    BlockHeader* block = block_from_ptr(ptr);
    if (block == NULL) {
        set_alloc_errno(PTR_NOT_TO_BLOCK_HEADER);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    if (block_is_free(block)) {
        set_alloc_errno(BLOCK_ALREADY_FREED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    block_mark_as_free(block);
    if ((block = controller_block_merge_prev(alloc->controller, block)) == NULL) {
        set_alloc_errno(MERGE_PREV_FAILED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    } else if ((block = controller_block_merge_next(alloc->controller, block)) == NULL) {
        set_alloc_errno(MERGE_NEXT_FAILED);
        __htfh_lock_unlock_handled(&alloc->mutex);
        return -1;
    }
    controller_block_insert(alloc->controller, block);
    __htfh_lock_unlock_handled(&alloc->mutex);
    return 0;
}