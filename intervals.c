#include "ugrep.h"

/*
TODO:
 - is it easier with a double linked list?
 - "garbage" old interval_t instead of alloc/free them each time
*/

static interval_t *interval_new(int32_t lower_limit, int32_t upper_limit)
{
    interval_t *i;

    i = mem_new(*i);
    i->lower_limit = lower_limit;
    i->upper_limit = upper_limit;

    return i;
}

static void interval_add_after(slist_t *UNUSED(intervals), slist_element_t *ref, int32_t lower_limit, int32_t upper_limit)
{
    slist_element_t *newel;

    newel = mem_new(*newel);
    newel->data = interval_new(lower_limit, upper_limit);
    newel->next = ref->next;
    ref->next = newel;
}

static void interval_append(slist_t *intervals, int32_t lower_limit, int32_t upper_limit)
{
    slist_append(intervals, interval_new(lower_limit, upper_limit));
}

#define BETWEEN(value, lower, upper) \
    ((lower <= value) && (value <= upper))

#define IN(value, interval) \
    (((value) >= (interval)->lower_limit) && ((value) <= (interval)->upper_limit))

UBool interval_add(slist_t *intervals, int32_t max_upper_limit, int32_t lower_limit, int32_t upper_limit)
{
    slist_element_t *prev, *from, *to;

    if (lower_limit == 0 && upper_limit == max_upper_limit) {
        return TRUE;
    }
    if (slist_empty(intervals)) {
        interval_append(intervals, lower_limit, upper_limit);
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
            interval_append(intervals, lower_limit, upper_limit);
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
                        if (NULL != intervals->dtor_func) {
                            intervals->dtor_func(tmp->data);
                        }
                        prev->next = tmp->next;
                        if (tmp == to) {
                            free(tmp);
                            break;
                        }
                        free(tmp);
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

slist_t *intervals_new(void)
{
    return slist_new(interval_destroy);
}
