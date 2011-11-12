#include "common.h"
#include "struct/intervals.h"

static dlist_element_t *dlist_pool_element_new(interval_list_t *, const void *) WARN_UNUSED_RESULT NONNULL();
static void dlist_pool_garbage_range(interval_list_t *, dlist_element_t *, dlist_element_t *) NONNULL();
static void interval_list_append(interval_list_t *, const void *) NONNULL();
static void interval_list_insert_before(interval_list_t *, const void *, dlist_element_t *) NONNULL();

interval_list_t *interval_list_new(void) /* WARN_UNUSED_RESULT */
{
    interval_list_t *l;

    l = mem_new(*l);
    l->elt_size = sizeof(interval_t);
    l->garbage = l->tail = l->head = NULL;
    l->len = 0;
#ifdef DEBUG
    l->recycled = 0;
#endif /* DEBUG */
    l->dtor_func = free;

    return l;
}

UBool interval_list_empty(interval_list_t *l) /* NONNULL() */
{
    require_else_return_false(NULL != l);

    return (NULL == l->head);
}

void interval_list_clean(interval_list_t *l) /* NONNULL() */
{
    require_else_return(NULL != l);

    if (NULL != l->tail) {
        if (NULL != l->garbage) {
            l->tail->next = l->garbage;
            l->garbage->prev = l->tail;
        }
        l->garbage = l->head;
        l->len = 0;
        l->head = l->tail = NULL;
    }
}

void interval_list_destroy(interval_list_t *l) /* NONNULL() */
{
    dlist_element_t *el;

    require_else_return(NULL != l);

    if (NULL != (el = l->head)) {
        while (NULL != el) {
            dlist_element_t *tmp = el;
            el = el->next;
            if (NULL != l->dtor_func) {
                l->dtor_func(tmp->data);
            }
            free(tmp);
        }
    }
    if (NULL != (el = l->garbage)) {
        while (NULL != el) {
            dlist_element_t *tmp = el;
            el = el->next;
            if (NULL != l->dtor_func) {
                l->dtor_func(tmp->data);
            }
            free(tmp);
        }
    }
    free(l);
}

static dlist_element_t *dlist_pool_element_new(interval_list_t *l, const void *src) /* WARN_UNUSED_RESULT NONNULL() */
{
    dlist_element_t *el;

    require_else_return_null(NULL != l);
    require_else_return_null(NULL != src);

    if (NULL != l->garbage) {
        el = l->garbage;
        l->garbage = el->next;
        if (NULL != el->prev) {
            el->prev->next = el->next;
        }
        if (NULL != el->next) {
            el->next->prev = el->prev;
        }
# ifdef DEBUG
        l->recycled++;
# endif /* DEBUG */
    } else {
        el = mem_new(*el);
        el->data = _mem_alloc(l->elt_size);
    }
    el->prev = el->next = NULL;
    memcpy(el->data, src, l->elt_size);

    return el;
}

static void interval_list_append(interval_list_t *l, const void *src) /* NONNULL() */
{
    dlist_element_t *el;

    require_else_return(NULL != l);
    require_else_return(NULL != src);

    el = dlist_pool_element_new(l, src);
    el->next = NULL;
    el->prev = l->tail;
    if (NULL == l->tail) {
        l->head = l->tail = el;
    } else {
        l->tail->next = el;
        l->tail = el;
    }
}

static void interval_list_insert_before(interval_list_t *l, const void *src, dlist_element_t *ref) /* NONNULL() */
{
    dlist_element_t *el;

    require_else_return(NULL != l);
    require_else_return(NULL != src);

    el = dlist_pool_element_new(l, src);
    el->prev = ref->prev;
    el->next = ref;
    if (NULL == ref->prev) {
         l->head = el;
    } else {
         ref->prev->next = el;
    }
    ref->prev = el;
}

static void dlist_pool_garbage_range(interval_list_t *l, dlist_element_t *from, dlist_element_t *to) /* NONNULL() */
{
    require_else_return(NULL != l);
    require_else_return(NULL != from);
    require_else_return(NULL != to);

    if (NULL != from->prev) {
        from->prev->next = to->next;
    }
    if (NULL != to->next) {
        to->next->prev = from->prev;
    }
    if (NULL != l->garbage) {
        l->garbage->prev = to;
    }
    from->prev = NULL;
    to->next = l->garbage;
    l->garbage = from;
}

/* Intervals are half-open: [lower_limit;upper_limit[ */
UBool interval_list_add(interval_list_t *intervals, int32_t max_upper_limit, int32_t lower_limit, int32_t upper_limit) /* NONNULL() */
{
    dlist_element_t *prev, *from, *to;
    interval_t n = {lower_limit, upper_limit};

    require_else_return_false(NULL != intervals);

    prev = from = to = NULL;
    if (lower_limit == 0 && upper_limit == max_upper_limit) {
        return TRUE;
    }
    if (interval_list_empty(intervals)) {
        interval_list_append(intervals, &n);
        return FALSE;
    } else {
        for (from = intervals->head, prev = NULL; NULL != from; from = from->next) {
            FETCH_DATA(from->data, i, interval_t);

            if (lower_limit < i->lower_limit) {
                if (upper_limit < i->upper_limit) {
                    interval_list_insert_before(intervals, &n, from);
                    return FALSE;
                } else {
                    break;
                }
            } else {
                if (lower_limit <= i->upper_limit) {
                    if (upper_limit <= i->upper_limit) {
                        return FALSE;
                    } else {
                        break;
                    }
                }
            }
            prev = from;
        }
        if (NULL == from) {
            interval_list_append(intervals, &n);
            return FALSE;
        } else {
            for (to = from->next, prev = from; NULL != to; to = to->next) {
                FETCH_DATA(to->data, i, interval_t);

                if (i->lower_limit >= upper_limit) {
                    to = prev;
                    break;
                }
                if (i->upper_limit >= upper_limit) {
                    break;
                }
                prev = to;
            }
            if (NULL == to) {
                to = prev;
            }
            {
                FETCH_DATA(to->data, t, interval_t);
                FETCH_DATA(from->data, f, interval_t);

                f->lower_limit = MIN(f->lower_limit, lower_limit);
                f->upper_limit = MAX(t->upper_limit, upper_limit);
                if (f->lower_limit == 0 && f->upper_limit == max_upper_limit) {
                    return TRUE;
                }
                if (from != to) {
                    dlist_pool_garbage_range(intervals, from->next, to);
                }
            }
        }
    }

    return FALSE;
}
