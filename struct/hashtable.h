#ifndef HASHTABLE_H

# define HASHTABLE_H

# define POINTER_TO_INT(p) ((int) (long) (p))
# define INT_TO_POINTER(i) ((void *) (long) (i))

typedef struct _Hashtable Hashtable;

typedef uint32_t (*func_hash_t)(const void *);
typedef int (*func_equal_t)(const void *, const void *);

void hashtable_destroy(Hashtable *) NONNULL();
UBool hashtable_empty(Hashtable *) NONNULL();
UBool hashtable_exists(Hashtable *, void *) NONNULL(1);
UBool hashtable_get(Hashtable *, void *, void **) NONNULL(1, 3);
Hashtable *hashtable_standalone_dup_new(func_hash_t, func_equal_t, size_t, size_t) WARN_UNUSED_RESULT NONNULL(1, 2);
Hashtable *hashtable_new(func_hash_t, func_equal_t, func_dtor_t, func_dtor_t, dup_t, dup_t) WARN_UNUSED_RESULT NONNULL(1, 2);
void hashtable_put(Hashtable *, void *, void *) NONNULL(1);
void hashtable_remove(Hashtable *, void *) NONNULL(1);
size_t hashtable_size(Hashtable *) NONNULL();
# ifdef DEBUG
void hashtable_debug(Hashtable *, toUString) NONNULL();
# endif /* DEBUG */

#endif /* !HASHTABLE_H */
