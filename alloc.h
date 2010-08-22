#ifndef ALLOC_H

# define ALLOC_H

# if GCC_VERSION >= 2096
#  define MALLOC __attribute__((malloc))
# else
#  define MALLOC
# endif /* MALLOC */

# if GCC_VERSION >= 4003
#  define ALLOC_SIZE(...) __attribute__((alloc_size(__VA_ARGS__)))
# else
#  define ALLOC_SIZE(...)
# endif /* ALLOC_SIZE */

# define mem_new(type)           _mem_alloc(sizeof(type))
# define mem_new_n(type, n)      _mem_alloc(sizeof(type) * (n))
# define mem_renew(ptr, type, n) _mem_realloc(ptr, sizeof(type) * (n))

void *_mem_alloc(size_t) MALLOC ALLOC_SIZE(1);
void *_mem_realloc(void *, size_t) MALLOC ALLOC_SIZE(2);

#endif /* !ALLOC_H */
