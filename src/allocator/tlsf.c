#include "tlsf.h"
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "../preprocessor/checks.h"
#include "../error/allocator_errno.h"

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

// ==== INTERNAL METHODS ====

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

// ==== PUBLIC API ====

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
    return __htfh_lock_unlock_handled(&alloc->mutex);
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
    if (block == NULL) {
        __htfh_lock_unlock_handled(&alloc->mutex);
        return NULL;
    }
    void* ptr = controller_mark_block_used(alloc->controller, block, fitted_allocation_size);
    __htfh_lock_unlock_handled(&alloc->mutex);
    return ptr;
}