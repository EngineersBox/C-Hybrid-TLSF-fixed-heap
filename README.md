# C-Hybrid-TLSF-fixed-heap
A hybrid TLSF fixed heap allocator for managing pre-allocated heap memory.

## Methods

| Signature                                                                   	  | Description                                                                                                                                                                                                                                                                                                                                              	   |
|--------------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `int htfh_new(Allocator* alloc)`                                             	 | Instantiates an allocated allocator with default values                                                                                                                                                                                                                                                                                                  	   |
| `int htfh_init(Allocator* alloc, AllocationMethod method, size_t heap_size)` 	 | Initialise memory mapped region for given heap size, utilising specified method for fragmentation resolution and allocation                                                                                                                                                                                                                              	   |
| `int htfh_destruct(Allocator* alloc)`                                        	 | Handled freeing of allocator with checking on heap state                                                                                                                                                                                                                                                                                                 	   |
| `void* htfh_malloc(Allocator* alloc, unsigned nbytes)`                       	 | Allocate memory from the mapped region for a given size                                                                                                                                                                                                                                                                                                  	   |
| `void* htfh_calloc(Allocator* alloc, unsigned count, unsigned nbytes)`       	 | Allocate contiguous memory from the mapped region for a given number of elements of given size                                                                                                                                                                                                                                                           	   |
| `void* htfh_realloc(Allocator* alloc, void* ap, unsigned nbytes)`            	 | Re-size a given block of memory to a new size, that was previously allocated by `htfh_malloc` or `htfh_calloc`.                                                                                                                                                                                                                                            	 |
| `void htfh_free(Allocator* alloc, void* ap)`                                 	 | Free the memory currently held by the provided pointer to a region of the mapped memory                                                                                                                                                                                                                                                                  	   |
| `void* htfh_sbrk(Allocator* alloc, intptr_t increment)`                      	 | Increments the data space by increment bytes. Calling `sbrk()` with an increment of 0 can be used to find the current location of the program break. On success, `sbrk()` returns the previous program break. (If the break was increased, then this value is a pointer to the start of the newly allocated memory). On error, `(void*) -1` is returned. 	   |
| `void* htfh_brk(Allocator* alloc, void* addr)`                               	 | Sets the end of the data segment to the value specified by `addr`, when that value is reasonable, the mapped region has enough memory, and the process does not exceed its maximum data size. On success, `brk()` returns `0`. On error, `-1` is returned.                                                                                               	   |

## Error Handling

Errors are handled in the same manner as standard usage of `perror(char* prefix)` follows, except with a custom method `alloc_perror(char* prefix)`.
Any call made with the HTFH methods, should follow with a return value check, in the case on an invalid/erroneous value, `alloc_perror(char* prefix)` should be called to display the error, file, line number and function name in stderr.

An example of an error being logged as a result of `alloc_perror("An error occured: ")` is as follows:

```
An error occured: Managed heap has already been allocated
	at main(/Users/EngineersBox/Desktop/Projects/C:C++/C-fixed-heap-allocator/src/main.c:25)
	at htfh_init(/Users/EngineersBox/Desktop/Projects/C:C++/C-fixed-heap-allocator/src/allocator/tlsf.c:112)
```

These locations correspond the following:

### Main.c:25

```c
// ... snip ...
if (htfh_init(alloc, 16 * 10000) != 0) {
    alloc_perror("An error occured: ");
    return 1;
}
// ... snip ...
```

### Tlsf.c:112

```c
// ... snip ...
} else if (alloc->heap != NULL) {
    set_alloc_errno(HEAP_ALREADY_MAPPED);
    return -1;
}
// ... snip ...
```

## Usage

There are two ways to utilise the allocator, one is through handled static construction and dynamic instantiation.

### Static

TODO

### Dynamic

TODO

# TODO

* Fix SIGSEGV with multiple malloc
* Fix SIGSEGV with malloc-free-malloc
* Implement calloc
* Implement realloc
* Implement memalign
