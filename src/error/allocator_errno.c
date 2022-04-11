#include "allocator_errno.h"
#include <string.h>

__thread int alloc_errno = NONE;
__thread char __alloc__errno_location[MAX_ERR_LINE_LENGTH];
__thread char __alloc__errno_msg[MAX_ERR_STRING_LENGTH];
__thread char __alloc__errno_strerr[MAX_ERR_LINE_LENGTH];

#define enum_error(enum_val, err_msg) case enum_val: strcpy(__alloc__errno_msg, err_msg); break;

inline void get_alloc_errmsg(AllocatorErrno err) {
    switch (err) {
        enum_error(NULL_ALLOCATOR_INSTANCE, "Allocator is not initialised")
        enum_error(HEAP_ALREADY_MAPPED, "Managed heap has already been allocated")
        enum_error(HEAP_MMAP_FAILED, "Failed to map memory for heap")
        enum_error(HEAP_UNMAP_FAILED, "Failed to unmap anonymous memory for heap")
        enum_error(BAD_DEALLOC, "Unable to destruct Allocator instance")
        enum_error(MALLOC_FAILED, "Unable to reserve memory")
        enum_error(MUTEX_LOCK_INIT, "Creation of mutex lock failed")
        enum_error(MUTEX_LOCK_LOCK, "Unable to lock allocator mutex")
        enum_error(MUTEX_LOCK_UNLOCK, "Unable to unlock allocator mutex")
        enum_error(MUTEX_LOCK_DESTROY, "Failed to destroy mutex lock")
        enum_error(PREV_BLOCK_FREE, "Previous block must be free")
        enum_error(BLOCK_IS_LAST, "Current block is last, next not present")
        enum_error(NEXT_BLOCK_NULL, "Next block is null")
        enum_error(PREV_BLOCK_NULL, "Previous block is null")
        enum_error(BLOCK_IS_NULL, "Block in context is null")
        enum_error(NON_ZERO_BLOCK_SIZE, "Block size must be non-zero")
        enum_error(ALIGN_POWER_OF_TWO, "Must align to a power of two")
        enum_error(NULL_CONTROLLER_INSTANCE, "Controller is not initialised")
        enum_error(SECOND_LEVEL_BITMAP_NULL, "Second level bitmap is null")
        enum_error(NONE, "")
        default: break;
    }
}
