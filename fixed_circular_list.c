#include "ugrep.h"

fixed_circular_list_t *fixed_circular_list_new(size_t length, func_ctor_t ctor_func, func_dtor_t dtor_func) /* WARN_UNUSED_RESULT NONNULL(1) */
{
    size_t i;
    fixed_circular_list_t *l;

    require_else_return_null(length >= 1);
    require_else_return_null(NULL != ctor_func);

    l = mem_new(*l);
    l->len = length;
    l->dtor_func = dtor_func;
    l->elts = mem_new_n(*l->elts, l->len);
    for (i = 0; i < l->len; i++) {
        //l->elts[i].prev = &l->elts[i - 1];
        l->elts[i].next = &l->elts[i + 1];
        l->elts[i].data = ctor_func();
        l->elts[i].used = FALSE;
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
    free(l->elts);
    free(l);
}

void fixed_circular_list_clean(fixed_circular_list_t *l) /* NONNULL() */
{
    size_t i;

    require_else_return(NULL != l);

    l->head = l->ptr;
    for (i = 0; i < l->len; i++) {
        l->elts[i].used = FALSE;
    }
}

void *fixed_circular_list_fetch(fixed_circular_list_t *l) /* NONNULL() */
{
    void *data;

    require_else_return_null(NULL != l);

    if (l->ptr->used && l->ptr == l->head) {
        l->head = l->head->next;
    }
    l->ptr->used = TRUE;
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

    return l->ptr == l->head && !l->ptr->used;
}

size_t fixed_circular_list_length(fixed_circular_list_t *l) /* NONNULL() */
{
    require_else_return_false(NULL != l);

    if (l->ptr == l->head) {
        return l->ptr->used ? l->len : 0;
    } else if (l->ptr > l->head) {
        return l->ptr - l->head;
    } else {
        return l->head - l->ptr;
    }
}

size_t fixed_circular_list_size(fixed_circular_list_t *l) /* NONNULL() */
{
    require_else_return_false(NULL != l);

    return l->len;
}
