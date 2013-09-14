#include "common.h"
#include "struct/intervals.h"

static dlist_element_t *dlist_pool_element_new(interval_list_t *, const void *) WARN_UNUSED_RESULT NONNULL();
static void dlist_pool_garbage_range(interval_list_t *, dlist_element_t *, dlist_element_t *) NONNULL();
static void interval_list_append(interval_list_t *, const void *) NONNULL();
static void interval_list_insert_before(interval_list_t *, const void *, dlist_element_t *) NONNULL();
static dlist_element_t *interval_list_shift(interval_list_t *) NONNULL();

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

static dlist_element_t *interval_list_shift(interval_list_t *l) /* NONNULL() */
{
    require_else_return_null(NULL != l);

    if (NULL != l->head) {
        dlist_element_t *el;

        el = l->head;
        if (el == l->tail) {
            l->head = l->tail = NULL;
        } else {
            l->head = l->head->next;
            l->head->prev = NULL;
        }
        if (NULL != l->garbage) {
            l->garbage->prev = el;
        }
        el->next = l->garbage;
        l->garbage = el;

        return l->head;
    } else {
        return NULL;
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
    if (l->tail == to) {
        l->tail = (NULL == from->prev ? NULL : from->prev);
    }
    if (NULL != to->next) {
        to->next->prev = from->prev;
    }
    if (l->head == from) {
        l->head = (NULL == to->next ? NULL : to->next);
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
        if (NULL != intervals->head) {
            if (intervals->head == intervals->tail) {
                FETCH_DATA(intervals->head->data, i, interval_t);

                i->lower_limit = lower_limit;
                i->upper_limit = upper_limit;
            } else {
                dlist_pool_garbage_range(intervals, intervals->head, intervals->tail);
                interval_list_append(intervals, &n);
            }
        } else {
            interval_list_append(intervals, &n);
        }
        return TRUE;
    }
    if (interval_list_empty(intervals)) {
        interval_list_append(intervals, &n);
        return FALSE;
    } else {
        for (from = intervals->head; NULL != from; from = from->next) {
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

void interval_list_complement(interval_list_t *intervals, int32_t min, int32_t max) /* NONNULL() */
{
    require_else_return(NULL != intervals);
    require_else_return(min < max);

    if (NULL == intervals->head) {
        interval_t n = {min, max};
        interval_list_append(intervals, &n);
    } else {
        int32_t l, lastu;
        dlist_element_t *current;

        lastu = min;
        current = intervals->head;
        {
            FETCH_DATA(current->data, i, interval_t);

            if (i->lower_limit <= min && i->upper_limit >= max) {
                dlist_pool_garbage_range(intervals, intervals->head, intervals->tail);
                return;
            }
        }
        while (NULL != current) {
            FETCH_DATA(current->data, i, interval_t);

            if (lastu > max) {
                break;
            }
            if (current == intervals->head && i->lower_limit <= min) {
                lastu = MAX(i->upper_limit, min);
                current = interval_list_shift(intervals);
            } else {
                l = i->lower_limit;
                i->lower_limit = lastu;
                lastu = i->upper_limit;
                i->upper_limit = MIN(l, max);
                current = current->next;
            }
        }
        if (NULL != current) {
            dlist_pool_garbage_range(intervals, current, intervals->tail);
        } else if (lastu < max) {
            interval_t n = {lastu, max};
            interval_list_append(intervals, &n);
        }
    }
}

UBool interval_list_is_bounded(interval_list_t *intervals) /* NONNULL() */
{
    require_else_return_false(NULL != intervals);

    if (NULL == intervals->tail) {
        return TRUE;
    } else {
        FETCH_DATA(intervals->tail->data, i, interval_t);

        return INT32_MAX != i->upper_limit;
    }
}

int32_t interval_list_length(interval_list_t *intervals) /* NONNULL() */
{
    int32_t length;
    dlist_element_t *el;

    require_else_return_val(NULL != intervals, -1);

    if (interval_list_is_bounded(intervals)) {
        for (el = intervals->head, length = 0; NULL != el; el = el->next) {
            FETCH_DATA(el->data, i, interval_t);

            length += i->upper_limit - i->lower_limit;
        }
    } else {
        length = -1;
    }

    return length;
}

#ifdef DEBUG
void interval_list_debug(interval_list_t *intervals) /* NONNULL() */
{
    dlist_element_t *el;

    require_else_return(NULL != intervals);

    for (el = intervals->head; NULL != el; el = el->next) {
        FETCH_DATA(el->data, i, interval_t);

        debug("[%d;%d[", i->lower_limit, i->upper_limit);
    }
}
#endif /* DEBUG */

const char *intervalParsingErrorName(int code)
{
    switch (code) {
        case FIELD_NO_ERR:
            return "no error";
        case FIELD_ERR_NUMBER_EXPECTED:
            return "number expected";
        case FIELD_ERR_OUT_OF_RANGE:
            return "number is out of the range [1;INT32_MAX[";
        case FIELD_ERR_NON_DIGIT_FOUND:
            return "non digit character found";
        case FIELD_ERR_INVALID_RANGE:
            return "invalid range: upper limit should be greater or equal than lower limit";
        default:
            return "bogus error code";
    }
}

#ifndef HAVE_STRCHRNUL
static char *strchrnul(const char *s, int c)
{
    while (('\0' != *s) && (*s != c)) {
        s++;
    }

    return (char *) s;
}
#endif /* HAVE_STRCHRNUL */

static int parseIntervalBoundary(const char *nptr, char **endptr, int32_t min, int32_t max, int32_t *ret)
{
    char c;
    const char *s;
    UBool negative;
    int any, cutlim;
    uint32_t cutoff, acc;

    s = nptr;
    acc = any = 0;
    if ('-' == (c = *s++)) {
        negative = TRUE;
        c = *s++;
    } else {
        negative = FALSE;
        if ('+' == *s) {
            c = *s++;
        }
    }
    cutoff = negative ? (uint32_t) - (INT32_MIN + INT32_MAX) + INT32_MAX : INT32_MAX;
    cutlim = cutoff % 10;
    cutoff /= 10;
    do {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else {
            break;
        }
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= 10;
            acc += c;
        }
    } while ('\0' != (c = *s++));
    if (NULL != endptr) {
        *endptr = (char *) (any ? s - 1 : nptr);
    }
    if (any < 0) {
        *ret = negative ? INT32_MIN : INT32_MAX;
        return FIELD_ERR_OUT_OF_RANGE;
    } else if (!any) {
        return FIELD_ERR_NUMBER_EXPECTED;
    } else if (negative) {
        *ret = -acc;
    } else {
        *ret = acc;
    }
    if (*ret < min || *ret > max) {
        *endptr = (char *) nptr;
        return FIELD_ERR_OUT_OF_RANGE;
    }

    return FIELD_NO_ERR;
}

UBool parseIntervals(error_t **error, const char *s, interval_list_t *intervals, int32_t min)
{
    int ret;
    char *endptr;
    const char *p, *comma;
    int32_t lower_limit, upper_limit;

    p = s;
    while ('\0' != *p) {
        lower_limit = min;
        upper_limit = INT32_MAX;
        comma = strchrnul(p, ',');
        if ('-' == *p) {
            /* -Y */
            if (FIELD_NO_ERR != (ret = parseIntervalBoundary(p + 1, &endptr, min, INT32_MAX, &upper_limit)) || ('\0' != *endptr && ',' != *endptr)) {
                error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                return FALSE;
            }
        } else {
            if (NULL == memchr(p, '-', comma - p)) {
                /* X */
                if (FIELD_NO_ERR != (ret = parseIntervalBoundary(p, &endptr, min, INT32_MAX, &lower_limit))/* || ('\0' != *endptr && ',' != *endptr)*/) {
                    error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                    return FALSE;
                }
                if ('\0' != *endptr && ',' != *endptr) {
                    error_set(error, FATAL, "digit or delimiter expected:\n%s\n%*c", s, endptr - s + 1, '^');
                    return FALSE;
                }
                upper_limit = lower_limit;
            } else {
                /* X- or X-Y */
                if (FIELD_NO_ERR != (ret = parseIntervalBoundary(p, &endptr, min, INT32_MAX, &lower_limit))) {
                    error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                    return FALSE;
                }
                if ('-' == *endptr) {
                    if ('\0' == *(endptr + 1)) {
                        // NOP (lower_limit = 0)
                    } else {
                        if (FIELD_NO_ERR != (ret = parseIntervalBoundary(endptr + 1, &endptr, min, INT32_MAX, &upper_limit)) || ('\0' != *endptr && ',' != *endptr)) {
                            error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                            return FALSE;
                        }
                    }
                } else {
                    error_set(error, FATAL, "'-' expected, get '%c' (%d):\n%s\n%*c", *endptr, *endptr, s, endptr - s + 1, '^');
                    return FALSE;
                }
            }
            if (lower_limit > upper_limit) {
                error_set(error, FATAL, "invalid interval: lower limit greater then upper one:\n%s\n%*c", s, p - s + 1, '^');
                return FALSE;
            }
        }
        debug("add [%d;%d[", lower_limit - min, upper_limit);
        interval_list_add(intervals, INT32_MAX, lower_limit - min, upper_limit); // - 1 because first index is 0 not 1
#if defined(DEBUG) && 0
        interval_list_debug(intervals);
#endif
        if ('\0' == *comma) {
            break;
        }
        p = comma + 1;
    }

    return TRUE;
}
