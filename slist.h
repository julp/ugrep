#ifndef SLIST_H

# define SLIST_H

typedef void (*func_dtor_t)(void *); /* Destructor callback */

typedef struct slist_element_t {
    struct slist_element_t *next;
    void *data;
} slist_element_t;

typedef struct {
    size_t len;
    slist_element_t *head;
    slist_element_t *queue;
    func_dtor_t dtor_func;
} slist_t;

void slist_append(slist_t *, void *);
void slist_destroy(slist_t *);
UBool slist_empty(slist_t *);
size_t slist_length(slist_t *);
slist_t *slist_new(func_dtor_t);

#endif /* !SLIST_H */
