#ifndef INTERVALS_H
# define INTERVALS_H

typedef struct {
    int32_t lower_limit;
    int32_t upper_limit;
} interval_t;

typedef struct dlist_element_t {
    struct dlist_element_t *next;
    struct dlist_element_t *prev;
    void *data;
} dlist_element_t;

typedef struct {
    size_t len;
    size_t elt_size;
# ifdef DEBUG
    size_t recycled;
# endif /* DEBUG */
    func_dtor_t dtor_func;
    dlist_element_t *head;
    dlist_element_t *tail;
    dlist_element_t *garbage;
} interval_list_t;

UBool interval_list_add(interval_list_t *, int32_t, int32_t, int32_t) NONNULL();
interval_list_t *interval_list_new(void) WARN_UNUSED_RESULT;
void interval_list_clean(interval_list_t *) NONNULL();
void interval_list_destroy(interval_list_t *) NONNULL();
UBool interval_list_empty(interval_list_t *) NONNULL();

#endif /* INTERVALS_H */
