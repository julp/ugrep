#include "ugrep.h"

slist_t *slist_new(func_dtor_t dtor_func) /* WARN_UNUSED_RESULT */
{
    slist_t *l;

    l = mem_new(*l);
    l->tail = l->head = NULL;
    l->len = 0;
    l->dtor_func = dtor_func;

    return l;
}

size_t slist_length(slist_t *l) /* NONNULL() */
{
    require_else_return_zero(NULL != l);

    return l->len;
}

UBool slist_empty(slist_t *l) /* NONNULL() */
{
    require_else_return_false(NULL != l);

    return (NULL == l->head);
}

void slist_append(slist_t *l, void *data) /* NONNULL(1) */
{
    slist_element_t *n;

    require_else_return(NULL != l);

    n = mem_new(*n);
    n->next = NULL;
    n->data = data;
    if (NULL == l->tail) {
        l->head = l->tail = n;
    } else {
        l->tail->next = n;
        l->tail = n;
    }
}

void slist_clean(slist_t *l) /* NONNULL() */
{
    slist_element_t *el;

    require_else_return(NULL != l);

    if (NULL != (el = l->head)) {
        while (NULL != el) {
            slist_element_t *tmp = el;
            el = el->next;
            if (NULL != l->dtor_func) {
                l->dtor_func(tmp->data);
            }
            free(tmp);
        }
        l->len = 0;
        l->tail = l->head = NULL;
    }
}

void slist_destroy(slist_t *l) /* NONNULL() */
{
    require_else_return(NULL != l);

    slist_clean(l);
    free(l);
}
