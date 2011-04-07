#include "common.h"

void *_mem_alloc(size_t size) /* MALLOC ALLOC_SIZE(1) */
{
    void *ptr;

    ptr = malloc(size);

    ensure(NULL != ptr);

    return ptr;
}

void *_mem_realloc(void *ptr, size_t size) /* MALLOC ALLOC_SIZE(2) */
{
    void *nptr;

    nptr = realloc(ptr, size);
    if (!nptr && ptr) {
        free(ptr);
    }

    ensure(NULL != nptr);

    return nptr;
}
