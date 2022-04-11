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

void* htfh_malloc(Allocator* alloc, size_t nbytes) {
    /* TODO:
     *  Adjust nbytes to fit minimum size requirements
     *  Find a free block
     *  Mark block as used and return
     */
    return NULL;
}