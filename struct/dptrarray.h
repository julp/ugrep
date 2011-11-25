#ifndef D_PTR_ARRAY_H

# define D_PTR_ARRAY_H

typedef struct _DPtrArray DPtrArray;

void *dptrarray_at(DPtrArray *, size_t) NONNULL();
void dptrarray_clear(DPtrArray *) NONNULL();
void dptrarray_destroy(DPtrArray *) NONNULL();
void dptrarray_insert(DPtrArray *, size_t, void *) NONNULL(1);
size_t dptrarray_length(DPtrArray *) NONNULL();
DPtrArray *dptrarray_new(dup_t, func_dtor_t) WARN_UNUSED_RESULT;
void *dptrarray_pop(DPtrArray *) NONNULL();
void dptrarray_push(DPtrArray *, void *) NONNULL(1);
void dptrarray_remove_at(DPtrArray *, size_t) NONNULL();
void dptrarray_remove_range(DPtrArray *, size_t, size_t) NONNULL();
void dptrarray_set_size(DPtrArray *, size_t) NONNULL();
void *dptrarray_shift(DPtrArray *) NONNULL();
DPtrArray *dptrarray_sized_new(size_t, dup_t, func_dtor_t) WARN_UNUSED_RESULT;
void dptrarray_swap(DPtrArray *, size_t, size_t) NONNULL();
void dptrarray_unshift(DPtrArray *, void *) NONNULL(1);

#endif /* !D_PTR_ARRAY_H */
