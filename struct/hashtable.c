#include "common.h"
#include "hashtable.h"

typedef struct {
    uint32_t hash;
    void *key;
    void *value;
} Bucket;

struct _Hashtable {
    size_t size;
    size_t count;
    uint8_t primeIndex;
    Bucket *buckets;
    func_hash_t hash_func;
    func_equal_t key_equal_func;
    func_dtor_t key_dtor_func;
    func_dtor_t value_dtor_func;
    dup_t key_duper;
    dup_t value_duper;
};

#define BUCKET_DELETED 0x80000000
#define BUCKET_EMPTY   (BUCKET_DELETED + 1)

#define BUCKET_EMPTY_OR_DELETED(b) \
    ((b).hash >= BUCKET_DELETED)

#define IS_BUCKET_EMPTY(b) \
    (BUCKET_EMPTY == (b).hash)

#define IS_BUCKET_DELETED(b) \
    (BUCKET_DELETED == (b).hash)

static const uint32_t PRIMES[] =
{
    13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749,
    65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593,
    16777213, 33554393, 67108859, 134217689, 268435399, 536870909,
    1073741789, 2147483647
};

// ./utr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 012abcDEF
// r abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ 012abcDEF
#ifdef DEBUG
# define DEFAULT_PRIMES_INDEX 0
#else
# define DEFAULT_PRIMES_INDEX 2
#endif /* DEBUG */
#define PRIMES_LENGTH         ARRAY_SIZE(PRIMES)

static uint32_t next_index(Hashtable *this, uint32_t h, int i)
{
    return (h % this->size + i * (1 + (h % (this->size - 2)))) % this->size;
}

static int get_bucket_index(Hashtable *this, void *key, uint32_t h, UBool ignoreDeletedBucket)
{
    int i, index;

    index = next_index(this, h, 0);
    for (
        i = 0;
        !IS_BUCKET_EMPTY(this->buckets[index])
        && (ignoreDeletedBucket || !IS_BUCKET_DELETED(this->buckets[index]))
        && !this->key_equal_func(key, this->buckets[index].key);
        i++
    ) {
        index = next_index(this, h, i);
    }

    return index;
}

static void maybe_rehash(Hashtable *this)
{
    require_else_return(NULL != this);

    if (this->count >= (size_t) (0.5 * this->size) && ((size_t) (this->primeIndex + 1) < PRIMES_LENGTH)) {
        int i;
        int index;
        size_t oldLength;
#ifdef DEBUG
        size_t oldCount;
#endif /* DEBUG */
        Bucket *oldBuckets;

#ifdef DEBUG
        oldCount = this->count;
#endif /* DEBUG */
        oldLength = this->size;
        oldBuckets = this->buckets;
        this->size = PRIMES[++this->primeIndex];
        this->buckets = mem_new_n(*this->buckets, this->size);
        for (i = 0; i < (int) this->size; i++) {
            this->buckets[i].hash = BUCKET_EMPTY;
#ifdef DEBUG
            this->buckets[i].key = NULL;
            this->buckets[i].value = NULL;
#endif /* DEBUG */
        }
#ifdef DEBUG
        this->count = 0;
#endif /* DEBUG */
        for (i = oldLength - 1; i >= 0; i--) {
            if (!BUCKET_EMPTY_OR_DELETED(oldBuckets[i])) {
                index = get_bucket_index(this, oldBuckets[i].key, oldBuckets[i].hash, FALSE);
                assert(IS_BUCKET_EMPTY(this->buckets[index]));
                this->buckets[index].hash = oldBuckets[i].hash;
                this->buckets[index].key = oldBuckets[i].key;
                this->buckets[index].value = oldBuckets[i].value;
#ifdef DEBUG
                ++this->count;
#endif /* DEBUG */
            }
        }
        free(oldBuckets);
        assert(this->count == oldCount);
    }
}

Hashtable *hashtable_standalone_dup_new(
    func_hash_t hash_func, func_equal_t key_equal_func,
    size_t key_size, size_t value_size
) /* WARN_UNUSED_RESULT NONNULL(1, 2) */ {
    return hashtable_new(hash_func, key_equal_func, free, free, SIZE_TO_DUP_T(key_size), SIZE_TO_DUP_T(value_size));
}

Hashtable *hashtable_new(
    func_hash_t hash_func, func_equal_t key_equal_func,
    func_dtor_t key_dtor_func, func_dtor_t value_dtor_func,
    dup_t key_duper, dup_t value_duper
) /* WARN_UNUSED_RESULT NONNULL(1, 2) */ {
    size_t i;
    Hashtable *this;

    require_else_return_null(NULL != hash_func);
    require_else_return_null(NULL != key_equal_func);

    this = mem_new(*this);
    this->count = 0;
    this->hash_func = hash_func;
    this->key_equal_func = key_equal_func;
    this->key_duper = key_duper;
    this->key_dtor_func = key_dtor_func;
    this->value_duper = value_duper;
    this->value_dtor_func = value_dtor_func;
    this->primeIndex = DEFAULT_PRIMES_INDEX;
    this->size = PRIMES[this->primeIndex];
    this->buckets = mem_new_n(*this->buckets, this->size);
    for (i = 0; i < this->size; i++) {
        this->buckets[i].hash = BUCKET_EMPTY;
#ifdef DEBUG
        this->buckets[i].key = NULL;
        this->buckets[i].value = NULL;
#endif /* DEBUG */
    }

    return this;
}

void hashtable_destroy(Hashtable *this) /* NONNULL() */
{
    size_t i;

    require_else_return(NULL != this);

    for (i = 0; i < this->size; i++) {
        if (!BUCKET_EMPTY_OR_DELETED(this->buckets[i])) {
            if (NULL != this->key_dtor_func) {
                this->key_dtor_func(this->buckets[i].key);
            }
            if (NULL != this->value_dtor_func) {
                this->value_dtor_func(this->buckets[i].value);
            }
        }
    }
    free(this->buckets);
    free(this);
}

void hashtable_put(Hashtable *this, void *key, void *value) /* NONNULL(1) */
{
    int index;
    uint32_t h;

    maybe_rehash(this);
    h = this->hash_func(key) & ~BUCKET_DELETED;
    index = get_bucket_index(this, key, h, FALSE);
    if (BUCKET_EMPTY_OR_DELETED(this->buckets[index])) {
        this->buckets[index].hash = h;
        this->buckets[index].key = clone(this->key_duper, key);
        ++this->count;
    } else {
        if (NULL != this->value_dtor_func) {
            this->value_dtor_func(this->buckets[index].value);
        }
    }
    this->buckets[index].value = clone(this->value_duper, value);
}

UBool hashtable_get(Hashtable *this, void *key, void **value) /* NONNULL(1, 3) */
{
    require_else_return_false(NULL != this);

    if (0 == this->count) {
        *value = NULL;
        return FALSE;
    } else {
        Bucket *b;
        uint32_t h;

        h = this->hash_func(key) & ~BUCKET_DELETED;
        b = &this->buckets[get_bucket_index(this, key, h, TRUE)];
        if (BUCKET_EMPTY_OR_DELETED(*b)) {
            *value = NULL;
            return FALSE;
        } else {
            *value = b->value;
            return TRUE;
        }
    }
}

void hashtable_remove(Hashtable *this, void *key) /* NONNULL(1) */
{
    require_else_return(NULL != this);

    if (this->count > 0) {
        Bucket *b;
        uint32_t h;

        h = this->hash_func(key) & ~BUCKET_DELETED;
        b = &this->buckets[get_bucket_index(this, key, h, TRUE)];
        if (!BUCKET_EMPTY_OR_DELETED(*b)) {
            if (NULL != this->key_dtor_func) {
                this->key_dtor_func(b->key);
            }
            if (NULL != this->value_dtor_func) {
                this->value_dtor_func(b->value);
            }
            b->hash = BUCKET_DELETED;
#ifdef DEBUG
            b->key = NULL;
            b->value = NULL;
#endif /* DEBUG */
            --this->count;
        }
    }
}

UBool hashtable_exists(Hashtable *this, void *key) /* NONNULL(1) */
{
    require_else_return_false(NULL != this);

    if (this->count > 0) {
        Bucket *b;
        uint32_t h;

        h = this->hash_func(key) & ~BUCKET_DELETED;
        b = &this->buckets[get_bucket_index(this, key, h, TRUE)];
        if (!BUCKET_EMPTY_OR_DELETED(*b)) {
            return TRUE;
        }
    }

    return FALSE;
}

size_t hashtable_size(Hashtable *this) /* NONNULL() */
{
    require_else_return_zero(NULL != this);

    return this->count;
}

UBool hashtable_empty(Hashtable *this) /* NONNULL() */
{
    require_else_return_true(NULL != this);

    return 0 == this->count;
}

#ifdef DEBUG
void hashtable_debug(Hashtable *this, toUString toustring) /* NONNULL() */
{
    size_t i;
    UString *ustr;

    ustr = ustring_new();
    for (i = 0; i < this->size; i++) {
        if (BUCKET_EMPTY_OR_DELETED(this->buckets[i])) {
            fprintf(stderr, "[%2d] %s\n", i, IS_BUCKET_DELETED(this->buckets[i]) ? "deleted" : "empty");
        } else {
            toustring(ustr, this->buckets[i].key, this->buckets[i].value);
            u_fprintf(ustderr, "[%2d] H = %u, %S\n", i, this->buckets[i].hash, ustr->ptr);
        }
    }
    ustring_destroy(ustr);
}
#endif /* DEBUG */
