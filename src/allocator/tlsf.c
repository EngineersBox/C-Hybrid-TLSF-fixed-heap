#include "tlsf.h"
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

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
    alloc->heap = mmap(
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
    return __htfh_lock_unlock_handled(&alloc->mutex);
}