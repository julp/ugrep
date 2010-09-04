#include "ugrep.h"

slist_pool_t *slist_pool_new(size_t elt_size, func_dtor_t dtor_func)
{
    slist_pool_t *l;

    l = mem_new(*l);
    l->elt_size = elt_size;
    l->garbage = l->tail = l->head = NULL;
    l->len = 0;
#ifdef DEBUG
    l->recycled = 0;
#endif
    l->dtor_func = dtor_func;

    return l;
}

static interval_t *interval_new(int32_t lower_limit, int32_t upper_limit)
{
    interval_t *i;

    i = mem_new(*i);
    i->lower_limit = lower_limit;
    i->upper_limit = upper_limit;

    return i;
}

UBool slist_pool_empty(slist_pool_t *l)
{
    return (NULL == l->head);
}

void slist_pool_clean(slist_pool_t *l)
{
    slist_element_t *el;

    if (NULL != (el = l->head)) {
        while (NULL != el) {
            slist_element_t *tmp = el;
            el = el->next;
            tmp->next = l->garbage;
            l->garbage = tmp;
        }
        l->len = 0;
        l->head = l->tail = NULL;
    }
}

void slist_pool_destroy(slist_pool_t *l)
{
    slist_element_t *el;

#ifdef DEBUG
    debug("%d elements was recycled by garbage", l->recycled);
#endif /* DEBUG */
    if (NULL != (el = l->head)) {
        while (NULL != el) {
            slist_element_t *tmp = el;
            el = el->next;
            if (NULL != l->dtor_func) {
                l->dtor_func(tmp->data);
            }
            free(tmp);
        }
    }
    if (NULL != (el = l->garbage)) {
        while (NULL != el) {
            slist_element_t *tmp = el;
            el = el->next;
            if (NULL != l->dtor_func) {
                l->dtor_func(tmp->data);
            }
            free(tmp);
        }
    }
    free(l);
}

static slist_element_t *slist_pool_element_new(slist_pool_t *l, const void *src)
{
    slist_element_t *el;

    // fetch element of garbage (and increment l->recycled) else alloc one
    if (NULL != l->garbage) {
        el = l->garbage;
        l->garbage = el->next;
        el->next = NULL;
#ifdef DEBUG
        l->recycled++;
#endif /* DEBUG */
    } else {
        el = mem_new(*el);
        el->next = NULL;
        el->data = _mem_alloc(l->elt_size);
    }

    memcpy(el->data, src, l->elt_size);

    return el;
}

void slist_pool_append(slist_pool_t *l, const void *src)
{
    slist_element_t *el;

    el = slist_pool_element_new(l, src);
    if (NULL == l->tail) {
        l->head = l->tail = el;
    } else {
        l->tail->next = el;
        l->tail = el;
    }
}

/*static void interval_add_after(slist_t *UNUSED(intervals), slist_element_t *ref, int32_t lower_limit, int32_t upper_limit)
{
    slist_element_t *newel;

    newel = mem_new(*newel);
    newel->data = interval_new(lower_limit, upper_limit);
    newel->next = ref->next;
    ref->next = newel;
}*/

static void interval_append(slist_t *intervals, int32_t lower_limit, int32_t upper_limit)
{
    slist_append(intervals, interval_new(lower_limit, upper_limit));
}

static void slist_pool_garbage_element(slist_pool_t *l, slist_element_t *target, slist_element_t *previous)
{
    previous->next = target->next;
    target->next = l->garbage->next;
    l->garbage = target;
#ifdef DEBUG
    l->recycled++;
#endif
}

#define BETWEEN(value, lower, upper) \
    ((lower <= value) && (value <= upper))

#define IN(value, interval) \
    (((value) >= (interval)->lower_limit) && ((value) <= (interval)->upper_limit))

#ifdef OLD_INTERVAL
UBool interval_add(slist_t *intervals, int32_t max_upper_limit, int32_t lower_limit, int32_t upper_limit)
#else
UBool interval_add(slist_pool_t *intervals, int32_t max_upper_limit, int32_t lower_limit, int32_t upper_limit)
#endif /* OLD_INTERVAL */
{
    slist_element_t *prev, *from, *to;

    if (lower_limit == 0 && upper_limit == max_upper_limit) {
        return TRUE;
    }
#ifdef OLD_INTERVAL
    if (slist_empty(intervals)) {
#else
    if (slist_pool_empty(intervals)) {
#endif /* OLD_INTERVAL */
#ifdef OLD_INTERVAL
        interval_append(intervals, lower_limit, upper_limit);
#else
        interval_t i = {lower_limit, upper_limit};
        slist_pool_append(intervals, &i);
#endif /* OLD_INTERVAL */
    } else {
        for (from = intervals->head, prev = NULL; from; from = from->next) {
            FETCH_DATA(from->data, i, interval_t);

            if (IN(lower_limit, i) || IN(upper_limit, i)) {
                break;
            }
            if (lower_limit < i->lower_limit) {
                break;
            }
            prev = from;
        }
        if (!from) {
#ifdef OLD_INTERVAL
            interval_append(intervals, lower_limit, upper_limit);
#else
            interval_t i = {lower_limit, upper_limit};
            slist_pool_append(intervals, &i);
#endif /* OLD_INTERVAL */
        } else {
            for (to = from->next, prev = NULL; to; to = to->next) {
                FETCH_DATA(to->data, i, interval_t);

                //if (!IN(lower_limit, i) && !IN(upper_limit, i)) {
                /*if (!BETWEEN(i->lower_limit, lower_limit, upper_limit) && !BETWEEN(i->upper_limit, lower_limit, upper_limit)) {
                    break;
                }
                if (i->upper_limit > upper_limit) {*/
                if (i->upper_limit >= upper_limit && !BETWEEN(i->lower_limit, lower_limit, upper_limit) && !BETWEEN(i->upper_limit, lower_limit, upper_limit)) {
                    to = prev;
                    break;
                }
                prev = to;
            }
            if (prev) {
                if (!to) {
                    to = prev;
                }
                {
                    slist_element_t *el;
                    FETCH_DATA(to->data, t, interval_t);
                    FETCH_DATA(from->data, f, interval_t);

                    f->lower_limit = MIN(f->lower_limit, lower_limit);
                    f->upper_limit = MAX(t->upper_limit, upper_limit);
                    if (f->lower_limit == 0 && f->upper_limit == max_upper_limit) {
                        return TRUE;
                    }
                    for (el = from->next, prev = from; el; ) {
                        slist_element_t *tmp = el;
                        el = el->next;
#ifdef OLD_INTERVAL
                        if (NULL != intervals->dtor_func) {
                            intervals->dtor_func(tmp->data);
                        }
                        prev->next = tmp->next;
                        if (tmp == to) {
                            free(tmp);
                            break;
                        }
                        free(tmp);
#else
                        slist_pool_garbage_element(intervals, tmp, prev);
                        if (tmp == to) {
                            break;
                        }
#endif /* OLD_INTERVAL */
                    }
                }
            } else {
                FETCH_DATA(from->data, i, interval_t);
                i->lower_limit = MIN(i->lower_limit, lower_limit);
                i->upper_limit = MAX(i->upper_limit, upper_limit);
                if (i->lower_limit == 0 && i->upper_limit == max_upper_limit) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

static void interval_destroy(void *data)
{
    free(data);
}

#ifdef OLD_INTERVAL
slist_t *intervals_new(void)
#else
slist_pool_t *intervals_new(void)
#endif /* OLD_INTERVAL */
{
#ifdef OLD_INTERVAL
    return slist_new(free/*interval_destroy*/);
#else
    return slist_pool_new(sizeof(interval_t), free);
#endif /* OLD_INTERVAL */
}
