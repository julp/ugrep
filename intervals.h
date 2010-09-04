#ifndef INTERVALS_H
# define INTERVALS_H

typedef struct {
    int32_t lower_limit;
    int32_t upper_limit;
} interval_t;

typedef struct {
    size_t len;
    size_t elt_size;
#ifdef DEBUG
    size_t recycled;
#endif
    func_dtor_t dtor_func;
    slist_element_t *head;
    slist_element_t *tail;
    slist_element_t *garbage;
} slist_pool_t;

#ifdef OLD_INTERVAL
UBool interval_add(slist_t *, int32_t, int32_t, int32_t);
slist_t *intervals_new(void);
#else
UBool interval_add(slist_pool_t *, int32_t, int32_t, int32_t);
slist_pool_t *intervals_new(void);
void slist_pool_append(slist_pool_t *, const void *);
void slist_pool_clean(slist_pool_t *);
void slist_pool_destroy(slist_pool_t *);
UBool slist_pool_empty(slist_pool_t *);
slist_pool_t *slist_pool_new(size_t, func_dtor_t);
#endif /* OLD_INTERVAL */

#endif /* INTERVALS_H */
