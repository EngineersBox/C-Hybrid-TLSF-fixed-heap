#include <stdlib.h>
#include "allocator/tlsf.h"
#include "error/allocator_errno.h"

struct TestStruct {
    int value;
    char str[4];
};

#define print_error(subs, bytes) \
    char *msg = calloc(100, sizeof(*msg)); \
    sprintf(msg, subs, bytes); \
    alloc_perror(msg); \
    free(msg); \
    return 1

#define HEAP_SIZE (16 * 10000)

static size_t align_up(size_t x, size_t align)
{
    tlsf_assert(0 == (align & (align - 1)) && "must align to a power of two");
    return (x + (align - 1)) & ~(align - 1);
}

static size_t adjust_request_size(size_t size, size_t align)
{
    size_t adjust = 0;
    if (size) {
        const size_t aligned = align_up(size, align);
        /* aligned sized must not exceed block_size_max or we'll go out of bounds on sl_bitmap */
        if (aligned < block_size_max) {
            adjust = aligned > block_size_min ? aligned : block_size_min;
        }
    }
    return adjust;
}


int main(int argc, char* argv[]) {
    Allocator* alloc = malloc(sizeof(*alloc));
    if (htfh_new(alloc) == -1) {
        alloc_perror("");
        return 1;
    }
    if (htfh_init(alloc, HEAP_SIZE) == -1) {
        alloc_perror("Initialisation failed for heap size 16*10000 bytes: ");
        return 1;
    }

    if (htfh_destruct(alloc) == -1) {
        alloc_perror("");
        return 1;
    }

    return 0;
}