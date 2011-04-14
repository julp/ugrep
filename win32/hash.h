#ifndef HASH_H

# define HASH_H

typedef struct _Hashtable Hashtable;
typedef void (*func_dtor_t)(void *);
typedef int32_t (*func_hash_t)(const void *);
typedef int (*func_equal_t)(const void *, const void *);

void hashtable_delete(Hashtable *, void *);
void hashtable_destroy(Hashtable *);
int hashtable_get(Hashtable *, void *, void **);
Hashtable *hashtable_new(func_hash_t, func_equal_t, func_dtor_t, func_dtor_t);
int hashtable_put(Hashtable *, void *, void *);

#endif /* !HASH_H */
