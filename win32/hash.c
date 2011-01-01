#include "ugrep.h"
#include "hash.h"

#define HASH_DELETED ((int32_t) 0x80000000)
#define HASH_EMPTY   ((int32_t) HASH_DELETED + 1)

#define IS_EMPTY_OR_DELETED(x) ((x) < 0)

typedef struct {
    int32_t hash;
    void *key;
    void *value;
} HashElement;

struct _Hashtable {
    size_t count;
    size_t length;
    uint8_t primeIndex;
    HashElement *elements;
    func_hash_t hash_func;
    func_equal_t key_equal_func;
    func_dtor_t key_dtor_func;
    func_dtor_t value_dtor_func;
};

static const uint32_t PRIMES[] = {
    13, 31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749,
    65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593,
    16777213, 33554393, 67108859, 134217689, 268435399, 536870909,
    1073741789, 2147483647 /*, 4294967291 */
};

#define PRIMES_LENGTH ARRAY_SIZE(PRIMES)
#define DEFAULT_PRIME_INDEX 3


static HashElement *hashtable_find(const Hashtable *ht, void *key, int32_t hashcode)
{
    int32_t firstDeleted = -1;
    int32_t theIndex, startIndex;
    int32_t jump = 0;
    int32_t tableHash;
    HashElement *elements = ht->elements;

    hashcode &= 0x7FFFFFFF;
    startIndex = theIndex = (hashcode ^ 0x4000000) % ht->length;

    do {
        tableHash = elements[theIndex].hash;
        if (tableHash == hashcode) {
            if (ht->key_equal_func(key, elements[theIndex].key)) {
                return &(elements[theIndex]);
            }
        } else if (!IS_EMPTY_OR_DELETED(tableHash)) {
        } else if (tableHash == HASH_EMPTY) {
            break;
        } else if (firstDeleted < 0) {
            firstDeleted = theIndex;
        }
        if (jump == 0) {
            jump = (hashcode % (ht->length - 1)) + 1;
        }
        theIndex = (theIndex + jump) % ht->length;
    } while (theIndex != startIndex);

    if (firstDeleted >= 0) {
        theIndex = firstDeleted;
    } else if (tableHash != HASH_EMPTY) {
        return NULL;
    }
    return &(elements[theIndex]);
}

static void hashtable_rehash(Hashtable *ht)
{
    int32_t newPrimeIndex;

    require_else_return(NULL != ht);

    newPrimeIndex = ht->primeIndex;
    if ((ht->count > (int32_t)(ht->length * 0.5F)) && (++newPrimeIndex < PRIMES_LENGTH)) {
        int32_t i;
        HashElement *old;
        int32_t oldLength;

        ht->count = 0;
        old = ht->elements;
        oldLength = ht->length;
        ht->primeIndex = newPrimeIndex;
        ht->length = PRIMES[ht->primeIndex];
        ht->elements = mem_renew(ht->elements, *ht->elements, ht->length);
        for (i = 0; i < ht->length; i++) {
            ht->elements[i].hash = HASH_EMPTY;
            ht->elements[i].key = ht->elements[i].value = NULL;
        }
        for (i = oldLength - 1; i >= 0; --i) {
            if (!IS_EMPTY_OR_DELETED(old[i].hash)) {
                HashElement *e = hashtable_find(ht, old[i].key, old[i].hash);
                e->key = old[i].key;
                e->value = old[i].value;
                e->hash = old[i].hash;
                ht->count++;
            }
        }

        free(old);
    }
}

Hashtable *hashtable_new(func_hash_t hash_func, func_equal_t key_equal_func, func_dtor_t key_dtor_func, func_dtor_t value_dtor_func)
{
    int i;
    Hashtable *ht;

    require_else_return_null(NULL != hash_func);
    require_else_return_null(NULL != key_equal_func);

    ht = mem_new(*ht);
    ht->hash_func = hash_func;
    ht->key_equal_func = key_equal_func;
    ht->key_dtor_func = key_dtor_func;
    ht->value_dtor_func = value_dtor_func;
    ht->count = 0;
    ht->primeIndex = DEFAULT_PRIME_INDEX;
    ht->length = PRIMES[ht->primeIndex];
    ht->elements = mem_new_n(*ht->elements, ht->length);
    for (i = 0; i < ht->length; i++) {
        ht->elements[i].hash = HASH_EMPTY;
        ht->elements[i].key = ht->elements[i].value = NULL;
    }

    return ht;
}

int hashtable_get(Hashtable *ht, void *key, void **data)
{
    HashElement *e;

    require_else_return_zero(NULL != ht);

    if (NULL == (e = hashtable_find(ht, key, ht->hash_func(key)))) {
        *data = NULL;
        return 0;
    } else {
        *data = e->value;
        return 1;
    }
}

void hashtable_delete(Hashtable *ht, void *key)
{
    HashElement* e;

    require_else_return(NULL != ht);

    e = hashtable_find(ht, key, ht->hash_func(key));
    if (!IS_EMPTY_OR_DELETED(e->hash)) {
        if (NULL != ht->key_dtor_func && NULL != e->key) {
                    ht->key_dtor_func(e->key);
        }
        if (NULL != ht->value_dtor_func && NULL != e->value) {
            ht->value_dtor_func(e->value);
        }
        e->hash = HASH_DELETED;
        ht->count--;
    }
}

int hashtable_put(Hashtable *ht, void *key, void *value)
{
    int32_t hashcode;
    HashElement* e;

    require_else_return_zero(NULL != ht);

    hashtable_rehash(ht);
    hashcode = ht->hash_func(key);
    e = hashtable_find(ht, key, hashcode);
    if (IS_EMPTY_OR_DELETED(e->hash)) {
        if (ht->count == ht->length) {
            return 0;
        } else {
            ht->count++;
        }
    }
    if (NULL != ht->key_dtor_func && NULL != e->key && e->key != key) {
        ht->key_dtor_func(e->key);
    }
    if (NULL != ht->value_dtor_func && e->value != NULL && e->value != value) {
        ht->value_dtor_func(e->value);
    }
    e->key = key;
    e->value = value;
    e->hash = hashcode & 0x7FFFFFFF;

    return 1;
}

void hashtable_destroy(Hashtable *ht)
{
    require_else_return(NULL != ht);

    if (NULL != ht->key_dtor_func || NULL != ht->value_dtor_func) {
        int i;

        for (i = 0; i < ht->length; i++) {
            if (!IS_EMPTY_OR_DELETED(ht->elements[i].hash)) {
                if (NULL != ht->key_dtor_func && NULL != ht->elements[i].key) {
                    ht->key_dtor_func(ht->elements[i].key);
                }
                if (NULL != ht->value_dtor_func && NULL != ht->elements[i].value) {
                    ht->value_dtor_func(ht->elements[i].value);
                }
            }
        }
    }
    free(ht->elements);

    free(ht);
}
