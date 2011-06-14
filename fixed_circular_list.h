#ifndef FIXED_CIRCULAR_LIST_H

# define FIXED_CIRCULAR_LIST_H

# ifdef OLD_RING
#  define fixed_circular_list_foreach(/*int */ index, /*fixed_circular_list_t **/l, /*flist_element_t **/el) \
    el = l->head; \
    for (index = fixed_circular_list_length(l) - 1; el->used && index >= 0; el = el->next, index--)
# else
#  define fixed_circular_list_foreach(/*int */ index, /*fixed_circular_list_t **/l, /*flist_element_t **/el) \
    el = l->head; \
    for (index = fixed_circular_list_length(l) - 1; *el->used && index >= 0; el = el->next, index--)
# endif /* OLD_RING */

typedef struct flist_element_t {
    struct flist_element_t *next;
    //struct flist_element_t *prev;
# ifdef OLD_RING
    UBool used;
# else
    UBool *used;
# endif
    void *data;
} flist_element_t;

typedef struct {
    size_t len;
    // union {
    //   struct { // size > 1
    flist_element_t *ptr;
    flist_element_t *head;
    flist_element_t *elts;
    //   } s;
    //   void *x; // size = 1
    // } u;
    func_dtor_t dtor_func;
    func_dtor_t clean_func;
# ifndef OLD_RING
    UBool *used;
# endif /* !OLD_RING */
} fixed_circular_list_t;

void fixed_circular_list_clean(fixed_circular_list_t *) NONNULL();
void fixed_circular_list_destroy(fixed_circular_list_t *) NONNULL();
UBool fixed_circular_list_empty(fixed_circular_list_t *) NONNULL();
void *fixed_circular_list_fetch(fixed_circular_list_t *) NONNULL();
size_t fixed_circular_list_length(fixed_circular_list_t *) NONNULL();
# ifdef OLD_RING
fixed_circular_list_t *fixed_circular_list_new(size_t, func_ctor_t, func_dtor_t, func_dtor_t) WARN_UNUSED_RESULT NONNULL(2);
# else
fixed_circular_list_t *fixed_circular_list_new(size_t, func_ctor_t, func_dtor_t) WARN_UNUSED_RESULT NONNULL(2);
# endif /* OLD_RING */
size_t fixed_circular_list_size(fixed_circular_list_t *) NONNULL();

#endif /* FIXED_CIRCULAR_LIST_H */
