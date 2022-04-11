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