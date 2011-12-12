#include "common.h"

void *_mem_alloc(size_t size) /* MALLOC ALLOC_SIZE(1) WARN_UNUSED_RESULT */
{
    void *ptr;

    ptr = malloc(size);

    ensure(NULL != ptr);

    return ptr;
}

void *_mem_realloc(void *ptr, size_t size) /* MALLOC ALLOC_SIZE(2) WARN_UNUSED_RESULT */
{
    void *nptr;

    nptr = realloc(ptr, size);
    if (!nptr && ptr) {
        free(ptr);
    }

    ensure(NULL != nptr);

    return nptr;
}

char *mem_dup(const char *string) /* WARN_UNUSED_RESULT */
{
    char *copy;
    size_t length;

    length = strlen(string) + 1;
    copy = mem_new_n(*copy, length);
    memcpy(copy, string, length);

    return copy;
}
