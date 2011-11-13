#include "common.h"
#include "dptrarray.h"

struct _DPtrArray {
    void **data;
    size_t length;
    size_t allocated;
    func_dtor_t dtor_func;
};

#define D_PTR_ARRAY_INCREMENT 8

static void dptrarray_maybe_resize_to(DPtrArray *this, size_t total_length) /* NONNULL() */
{
    assert(NULL != this);

    if (total_length >= this->allocated) {
        this->allocated = ((total_length / D_PTR_ARRAY_INCREMENT) + 1) * D_PTR_ARRAY_INCREMENT;
        this->data = mem_renew(this->data, *this->data, this->allocated);
    }
}

static void dptrarray_maybe_resize_of(DPtrArray *this, size_t additional_length) /* NONNULL() */
{
    assert(NULL != this);

    dptrarray_maybe_resize_to(this, this->length + additional_length);
}

DPtrArray *dptrarray_sized_new(size_t length, func_dtor_t dtor_func) /* WARN_UNUSED_RESULT */
{
    DPtrArray *this;

    this = mem_new(*this);
    this->data = NULL;
    this->length = this->allocated = 0;
    dptrarray_maybe_resize_to(this, length);
    this->dtor_func = dtor_func;

    return this;
}

DPtrArray *dptrarray_new(func_dtor_t dtor_func) /* WARN_UNUSED_RESULT */
{
    return dptrarray_sized_new(D_PTR_ARRAY_INCREMENT, dtor_func);
}

void dptrarray_destroy(DPtrArray *this) /* NONNULL() */
{
    assert(NULL != this);

    if (NULL != this->dtor_func) {
        size_t i;

        for (i = 0; i < this->length; i++) {
            this->dtor_func(this->data[i]);
        }
    }
    free(this->data);
    free(this);
}

void dptrarray_clear(DPtrArray *this) /* NONNULL() */
{
    assert(NULL != this);

    if (NULL != this->dtor_func) {
        size_t i;

        for (i = 0; i < this->length; i++) {
            this->dtor_func(this->data[i]);
        }
    }
    this->length = 0;
}

void *dptrarray_pop(DPtrArray *this) /* NONNULL() */
{
    assert(this->length > 0);

    return this->data[--this->length];
}

void *dptrarray_shift(DPtrArray *this) /* NONNULL() */
{
    void *data;

    assert(this->length > 0);

    data = this->data[0];
    memmove(this->data, this->data + 1, (this->length - 1) * sizeof(*this->data));
    --this->length;

    return data;
}

void dptrarray_push(DPtrArray *this, void *data) /* NONNULL(1) */
{
    assert(NULL != this);

    dptrarray_maybe_resize_of(this, 1);
    this->data[this->length++] = data;
}

void dptrarray_unshift(DPtrArray *this, void *data) /* NONNULL(1) */
{
    assert(NULL != this);

    dptrarray_maybe_resize_of(this, 1);
    memmove(this->data + 1, this->data, this->length * sizeof(*this->data));
    this->data[0] = data;
    ++this->length;
}

void dptrarray_insert(DPtrArray *this, size_t offset, void *data) /* NONNULL(1) */
{
    assert(NULL != this);
    assert(offset <= this->length);

    dptrarray_maybe_resize_of(this, 1);
    if (offset != this->length) {
        memmove(this->data + offset + 1, this->data + offset, (this->length - offset) * sizeof(*this->data));
    }
    this->data[offset] = data;
    ++this->length;
}

void dptrarray_remove_at(DPtrArray *this, size_t offset) /* NONNULL() */
{
    assert(NULL != this);
    assert(offset < this->length);

    if (NULL != this->dtor_func) {
        this->dtor_func(this->data[offset]);
    }
    memmove(this->data + offset, this->data + offset + 1, (this->length - offset - 1) * sizeof(*this->data));
    --this->length;
}

void dptrarray_remove_range(DPtrArray *this, size_t from, size_t to) /* NONNULL() */
{
    size_t diff;

    assert(NULL != this);
    assert(from < this->length);
    assert(to < this->length);
    assert(from <= to);

    diff = to - from + 1;
    if (NULL != this->dtor_func) {
        size_t i;

        for (i = from; i <= to; i++) {
            this->dtor_func(this->data[i]);
        }
    }
    memmove(this->data + to + 1, this->data + from, (this->length - diff) * sizeof(*this->data));
    this->length -= diff;
}

void *dptrarray_at(DPtrArray *this, size_t offset) /* NONNULL() */
{
    assert(NULL != this);

    if (offset < this->length) {
        return this->data[offset];
    } else {
        return NULL;
    }
}

void dptrarray_swap(DPtrArray *this, size_t offset1, size_t offset2) /* NONNULL() */
{
    void *tmp;

    assert(NULL != this);
    assert(offset1 < this->length);
    assert(offset2 < this->length);

    tmp = this->data[offset1];
    this->data[offset1] = this->data[offset2];
    this->data[offset2] = tmp;
}

void dptrarray_set_size(DPtrArray *this, size_t length) /* NONNULL() */
{
    assert(NULL != this);

    if (length < this->length) {
        if (NULL != this->dtor_func) {
            size_t i;

            for (i = length - 1; i < this->length; i++) {
                this->dtor_func(this->data[i]);
            }
        }
        this->length = length;
    } else {
        dptrarray_maybe_resize_to(this, length);
    }
}

size_t dptrarray_length(DPtrArray *this) /* NONNULL() */
{
    assert(NULL != this);

    return this->length;
}
