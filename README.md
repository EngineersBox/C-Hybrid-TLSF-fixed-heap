# C-Hybrid-TLSF-fixed-heap

C UNIX only hybrid TLSF fixed heap allocator for managing pre-allocated heap memory via an anonymous memory map.

## Credits

Implementation is based on Matt Conte's TLSF: <https://github.com/mattconte/tlsf>

## Methods

| Signature                                                                   	             | Description                                                                                                                                                                                                                                                                                                                                              	                                                           |
|-------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Allocator* alloc htfh_create(size_t bytes)`                                             	 | Instantiates an new allocator with default values and creates an anonymous memory map of size `bytes` as the heap                                                                                                                                                                                                                                                                                                  	 |
| `int htfh_destroy(Allocator* alloc)`                                        	               | Handled freeing of allocator with checking on heap state                                                                                                                                                                                                                                                                                                 	                                                           |
| `void* htfh_malloc(Allocator* alloc, unsigned nbytes)`                       	            | Allocate memory from the mapped region for a given size                                                                                                                                                                                                                                                                                                  	                                                           |
| `void* htfh_calloc(Allocator* alloc, unsigned count, unsigned nbytes)`       	            | Allocate contiguous memory from the mapped region for a given number of elements of given size                                                                                                                                                                                                                                                           	                                                           |
| `void* htfh_realloc(Allocator* alloc, void* ap, unsigned nbytes)`            	            | Re-size a given block of memory to a new size, that was previously allocated by `htfh_malloc` or `htfh_calloc`.                                                                                                                                                                                                                                            	                                                         |
| `void htfh_free(Allocator* alloc, void* ap)`                                 	            | Free the memory currently held by the provided pointer to a region of the mapped memory                                                                                                                                                                                                                                                                  	                                                           |

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
    __htfh_lock_unlock_handled(&alloc->mutex);
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
