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

enum {
    FIELD_NO_ERR = 0,
    FIELD_ERR_NUMBER_EXPECTED, // s == *endptr
    FIELD_ERR_OUT_OF_RANGE,    // number not in [min;max] ([1;INT_MAX] here)
    FIELD_ERR_NON_DIGIT_FOUND, // *endptr not in ('\0', ',')
    FIELD_ERR_INVALID_RANGE,   // lower_limit > upper_limit
    FIELD_ERR__COUNT
};

UBool interval_list_add(interval_list_t *, int32_t, int32_t, int32_t) NONNULL();
void interval_list_clean(interval_list_t *) NONNULL();
void interval_list_complement(interval_list_t *, int32_t, int32_t) NONNULL();
# ifdef DEBUG
void interval_list_debug(interval_list_t *) NONNULL();
# endif /* DEBUG */
void interval_list_destroy(interval_list_t *) NONNULL();
UBool interval_list_empty(interval_list_t *) NONNULL();
interval_list_t *interval_list_new(void) WARN_UNUSED_RESULT;

const char *intervalParsingErrorName(int);
UBool parseIntervals(error_t **, const char *, interval_list_t *, int32_t);

#endif /* INTERVALS_H */
