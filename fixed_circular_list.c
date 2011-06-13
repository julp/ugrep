#include "ugrep.h"

#ifdef OLD_RING
fixed_circular_list_t *fixed_circular_list_new(size_t length, func_ctor_t ctor_func, func_dtor_t dtor_func, func_dtor_t clean_func) /* WARN_UNUSED_RESULT NONNULL(2) */
#else
fixed_circular_list_t *fixed_circular_list_new(size_t length, func_ctor_t ctor_func, func_dtor_t dtor_func) /* WARN_UNUSED_RESULT NONNULL(2) */
#endif /* OLD_RING */
{
    size_t i;
    fixed_circular_list_t *l;

    require_else_return_null(length >= 1);
    require_else_return_null(NULL != ctor_func);

    l = mem_new(*l);
    l->len = length;
    l->dtor_func = dtor_func;
    l->elts = mem_new_n(*l->elts, l->len);
#ifdef OLD_RING
    l->clean_func = clean_func;
#else
    l->used = mem_new_n(*l->used, l->len);
#endif /* OLD_RING */
    for (i = 0; i < l->len; i++) {
        //l->elts[i].prev = &l->elts[i - 1];
        l->elts[i].next = &l->elts[i + 1];
        l->elts[i].data = ctor_func();
#ifdef OLD_RING
        l->elts[i].used = FALSE;
#else
        l->used[i] = FALSE;
        l->elts[i].used = &l->used[i];
#endif /* OLD_RING */
    }
    //l->elts[0].prev = &l->elts[l->len - 1];
    l->elts[l->len - 1].next = &l->elts[0];

    l->head = l->ptr = &l->elts[0];

    return l;
}

void fixed_circular_list_destroy(fixed_circular_list_t *l) /* NONNULL() */
{
    require_else_return(NULL != l);

    if (NULL != l->dtor_func) {
        size_t i;

        for (i = 0; i < l->len; i++) {
            l->dtor_func(l->elts[i].data);
        }
    }
#ifndef OLD_RING
    free(l->used);
#endif /* !OLD_RING */
    free(l->elts);
    free(l);
}

void fixed_circular_list_clean(fixed_circular_list_t *l) /* NONNULL() */
{
#ifdef OLD_RING
    size_t i;
#endif /* OLD_RING */

    require_else_return(NULL != l);

    l->head = l->ptr;
#ifdef OLD_RING
    for (i = 0; i < l->len; i++) {
        l->elts[i].used = FALSE;
        if (NULL != l->clean_func) {
            l->clean_func(l->elts[i].data);
        }
    }
#else
    memset(l->used, FALSE, l->len);
#endif /* OLD_RING */
}

void *fixed_circular_list_fetch(fixed_circular_list_t *l) /* NONNULL() */
{
    void *data;

    require_else_return_null(NULL != l);

#ifdef OLD_RING
    if (l->ptr->used && l->ptr == l->head) {
#else
    if (*l->ptr->used && l->ptr == l->head) {
#endif /* OLD_RING */
        l->head = l->head->next;
    }
#ifdef OLD_RING
    l->ptr->used = TRUE;
#else
    *l->ptr->used = TRUE;
#endif /* OLD_RING */
    data = l->ptr->data;
    l->ptr = l->ptr->next;
    /*if (l->ptr == l->head) {
        l->head = l->head->next;
    }*/
    //l->ptr = l->ptr->prev;

    return data;
    //return l->ptr->prev->data;
    //return l->ptr->next->data;
}

UBool fixed_circular_list_empty(fixed_circular_list_t *l) /* NONNULL() */
{
    require_else_return_false(NULL != l);

#ifdef OLD_RING
    return l->ptr == l->head && !l->ptr->used;
#else
    return l->ptr == l->head && !*l->ptr->used;
#endif /* OLD_RING */
}

size_t fixed_circular_list_length(fixed_circular_list_t *l) /* NONNULL() */
{
    require_else_return_false(NULL != l);

    if (l->ptr == l->head) {
#ifdef OLD_RING
        return l->ptr->used ? l->len : 0;
#else
        return *l->ptr->used ? l->len : 0;
#endif /* OLD_RING */
    } else if (l->ptr > l->head) {
        return l->ptr - l->head;
    } else {
        return (l->len - (l->head - l->ptr));
    }
}

size_t fixed_circular_list_size(fixed_circular_list_t *l) /* NONNULL() */
{
    require_else_return_false(NULL != l);

    return l->len;
}
