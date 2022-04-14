#include <stdlib.h>
#include "allocator/tlsf.h"
#include "error/allocator_errno.h"

struct TestStruct {
    int value;
    char str[18];
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
    if (htfh_new(alloc) != 0) {
        alloc_perror("");
        return 1;
    }
    if (htfh_init(alloc, HEAP_SIZE) != 0) {
        alloc_perror("Initialisation failed for heap size 16*10000 bytes: ");
        return 1;
    }
    struct TestStruct* test_struct = htfh_malloc(alloc, sizeof(*test_struct));
    if (test_struct == NULL) {
        print_error("Failed to allocate %zu bytes for TestStruct: ", sizeof(*test_struct));
    }
    test_struct->value = 42;
    strncpy(test_struct->str, "abcdefghijklmnopqr", 18);

    printf("Test struct:    [Value: %d] [Str: %s]\n", test_struct->value, test_struct->str);

//    if (htfh_free(alloc, test_struct) != 0) {
//        alloc_perror("Failed to deallocate TestStruct: ");
//        return 1;
//    }

    struct TestStruct* test_struct2 = htfh_malloc(alloc, sizeof(*test_struct2));
    if (test_struct2 == NULL) {
        print_error("Failed to allocate %zu bytes for TestStruct2: ", sizeof(*test_struct2));
    }
    test_struct2->value = 84;
    strncpy(test_struct2->str, "012345678901234567", 18);

    printf("Test struct 2: [Value: %d] [Str: %s]\n", test_struct2->value, test_struct2->str);
    printf("Test struct:   [Value: %d] [Str: %s]\n", test_struct->value, test_struct->str);

//    if (htfh_free(alloc, test_struct2) != 0) {
//        alloc_perror("Failed to deallocate TestStruct2: ");
//        return 1;
//    }

    if (htfh_destruct(alloc) != 0) {
        alloc_perror("");
        return 1;
    }

    return 0;
}