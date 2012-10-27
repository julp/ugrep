#ifndef DARRAY_H

# define DARRAY_H

typedef struct {
    uint8_t *data;
    size_t length;
    size_t allocated;
    size_t element_size;
} DArray;

# define darray_prepend(/*DArray **/ da, value) \
    darray_prepend((da), &(value), 1)

# define darray_push(/*DArray **/ da, value) \
    darray_append((da), (value))

# define darray_append(/*DArray **/ da, value) \
    darray_append_all((da), &(value), 1)

# define darray_insert(/*DArray **/ da, /*uint*/ offset, value) \
    darray_insert((da), (offset), &(value), 1)

# define darray_at_unsafe(/*DArray **/ da, /*uint*/ offset, T) \
    ((T *) ((void *) (da)->data))[(offset)]

void darray_append_all(DArray *, const void * const, size_t);
UBool darray_at(DArray *, uint, void *);
void darray_clear(DArray *);
void darray_destroy(DArray *);
void darray_insert_all(DArray *, uint, const void * const, size_t);
size_t darray_length(DArray *);
DArray *darray_new(size_t);
UBool darray_pop(DArray *, void *);
void darray_prepend_all(DArray *, const void * const, size_t);
UBool darray_remove_at(DArray *, uint);
void darray_remove_range(DArray *, uint, uint);
void darray_set_size(DArray *, size_t);
UBool darray_shift(DArray *, void *);
DArray *darray_sized_new(size_t, size_t);
void darray_swap(DArray *, uint, uint);

#endif /* !DARRAY_H */
