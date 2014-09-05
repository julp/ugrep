#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <unicode/ucal.h>
#include <unicode/udat.h>
#include <unicode/ucol.h>

#include "common.h"
#include "engine.h"
#include "parsenum.h"
#include "struct/rbtree.h"
#include "struct/darray.h"
#include "struct/dptrarray.h"


enum {
    USORT_EXIT_SUCCESS = 0,
    USORT_EXIT_FAILURE,
    USORT_EXIT_USAGE
};

typedef struct {
    uint64_t implemented;
    void *(*create)(uint64_t flags, error_t **error);
    UBool (*equals)(const void *object, uint64_t flags);
    RBKey *(*keygen)(const void *object, const UChar *source, int32_t sourceLength, error_t **error);
} Sorter;

typedef struct {
    int32_t order;
    int32_t start_field;
    int32_t end_field;
    int32_t start_offset;
    int32_t end_offset;
    uint64_t options; // bdfgiMhnRrV
    Sorter *sorter;
    void *private;
} USortField;

typedef struct {
    UString *line;
    uint32_t count;
    /*uint8_t **/RBKey *keys; /* TODO: array of keys (each piece/field has a key) */
} Line;

#define USORT_OPT_IGNORE_LEAD_BLANKS (1 << 0)
#define USORT_OPT_DICTIONNARY_ORDER  (1 << 1)
#define USORT_OPT_IGNORE_CASE        (1 << 2)
#define USORT_OPT_IGNORE_NON_PRINT   (1 << 3)
#define USORT_OPT_SORT_MASK          (~(USORT_OPT_IGNORE_LEAD_BLANKS | USORT_OPT_DICTIONNARY_ORDER | USORT_OPT_IGNORE_CASE | USORT_OPT_IGNORE_NON_PRINT))
#define USORT_OPT_NUM_SORT           (1 << 4)
#define USORT_OPT_DEFAULT_SORT       (1 << 5)
#define USORT_OPT_GENERAL_NUM_SORT   (1 << 6)
#define USORT_OPT_MONTH_SORT         (1 << 7)
#define USORT_OPT_HUMAN_SORT         (1 << 8)
#define USORT_OPT_RANDOM_SORT        (1 << 9)
#define USORT_OPT_VERSION_SORT       (1 << 10)
#define USORT_OPT_IP_ADDRESS_SORT    (1 << 11)

/* ========== global variables ========== */

static const UChar DEFAULT_SEPARATOR[] = { U_HT, 0 };
static const USortField INIT_USORT_FIELD = { 0, 0, INT32_MAX, 0, INT32_MAX, 0, NULL, NULL };

extern engine_t fixed_engine;
// extern engine_t re_engine;

static RBTree *tree = NULL;
static UString *ustr = NULL;
static DArray *pieces = NULL;
static DPtrArray *fields = NULL;
static UString *separator = NULL;
static USortField **machine_ordered_fields = NULL;
static pattern_data_t pdata = { NULL, &fixed_engine };
static func_cmp_t cmp_func = ucol_key_cmp;

static UBool bFlag = FALSE;
static UBool uFlag = FALSE;

static uint64_t global_options = 0;

/* ========== getopt stuff ========== */

enum {
    BINARY_OPT = GETOPT_SPECIFIC,
    MIN_OPT,
    MAX_OPT,
    SORT_OPT,
    VERSION_OPT
};

enum {
    MIN_ONLY,
    MAX_ONLY,
    ALL
};

static char optstr[] = "bfghik:MnRrt:uV";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"binary-files",          required_argument, NULL, BINARY_OPT},
    {"max",                   no_argument,       NULL, MAX_OPT},
    {"min",                   no_argument,       NULL, MIN_OPT},
    {"sort",                  required_argument, NULL, SORT_OPT},
    {"version",               no_argument,       NULL, VERSION_OPT},
    {"ignore-leading-blanks", no_argument,       NULL, 'b'},
    {"ignore-case",           no_argument,       NULL, 'f'},
    {"general-numeric-sort",  no_argument,       NULL, 'g'},
    {"humanc-numeric-sort",   no_argument,       NULL, 'h'},
    {"ip-address-sort",       no_argument,       NULL, 'i'},
    {"key",                   required_argument, NULL, 'k'},
    {"month-sort",            no_argument,       NULL, 'M'},
    {"numeric-sort",          no_argument,       NULL, 'n'},
    {"random-sort",           no_argument,       NULL, 'R'},
    {"reverse",               no_argument,       NULL, 'r'},
    {"field-separator",       required_argument, NULL, 't'},
    {"unique",                no_argument,       NULL, 'u'},
    {"version-sort",          no_argument,       NULL, 'V'},
    {NULL,                    no_argument,       NULL, 0}
};

struct {
    const char *name;
    int short_opt_val;
    uint64_t flag_value;
} static sortnames[] = {
    {"general-numeric", 'g', USORT_OPT_GENERAL_NUM_SORT},
    {"human-numeric",   'h', USORT_OPT_HUMAN_SORT},
    {"ip",              'i', USORT_OPT_IP_ADDRESS_SORT},
    {"month",           'M', USORT_OPT_MONTH_SORT},
    {"numeric",         'n', USORT_OPT_NUM_SORT},
    {"random",          'R', USORT_OPT_RANDOM_SORT},
    {"version",         'V', USORT_OPT_VERSION_SORT}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s [-%s] [file ...]\n",
        __progname,
        optstr
    );
    exit(USORT_EXIT_USAGE);
}

/* ========== sort engines ========== */

static void *month_sorter_create(uint64_t UNUSED(flags), error_t **error)
{
    UDateFormat *df;
    UErrorCode status;
    UChar format[] = { 0x4D, 0x4D, 0x4D, 0x4D, 0 }; /* "MMMM" */

    status = U_ZERO_ERROR;
    df = udat_open(UDAT_IGNORE, UDAT_IGNORE, NULL, NULL, -1, format, STR_SIZE(format), &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "udat_open");
    } else {
        env_register_resource(df, (func_dtor_t) udat_close);
    }

    return df;
}

static UBool month_sorter_equals(const void *UNUSED(object), uint64_t flags)
{
    return HAS_FLAG(flags, USORT_OPT_MONTH_SORT);
}

/* compare (unknown) < 'JAN' < ... < 'DEC' */
static RBKey *month_sorter_keygen(const void *object, const UChar *source, int32_t sourceLength, error_t **error)
{
    int32_t month;
//     uint8_t *result;
    RBKey *result;
    UDateFormat *df;
    UErrorCode status;
    static UCalendar *ucal = NULL;

    status = U_ZERO_ERROR;
    df = (UDateFormat *) object;
    if (NULL == ucal) {
        ucal = ucal_open(NULL, -1, NULL, UCAL_GREGORIAN, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "udat_parse");
            return NULL;
        }
        env_register_resource(ucal, (func_dtor_t) ucal_close);
    }
    udat_parseCalendar(df, ucal, source, sourceLength, NULL, &status);
    if (U_FAILURE(status)) {
        // month = 0; ?
        icu_error_set(error, FATAL, status, "udat_parse");
    }
    month = ucal_get(ucal, UCAL_MONTH, &status) + 1;
    if (U_FAILURE(status)) {
        // month = 0; ?
        icu_error_set(error, FATAL, status, "ucal_get");
    }
//     result = mem_new_n(*result, 2);
//     result[0] = month;
//     result[1] = 0;
    result = mem_new(*result);
    result->key = mem_new(*result->key);
    *result->key = month;
    result->key_len = 1;

    return result;
}

static void *ucol_sorter_create(uint64_t flags, error_t **error)
{
    UCollator *ucol;
    UErrorCode status;

    status = U_ZERO_ERROR;
    ucol = ucol_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucol_open");
    } else {
        env_register_resource(ucol, (func_dtor_t) ucol_close);
        if (HAS_FLAG(flags, USORT_OPT_NUM_SORT)) {
            ucol_setAttribute(ucol, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
            if (U_FAILURE(status)) {
                icu_error_set(error, FATAL, status, "ucol_setAttribute");
            }
        }
        if (HAS_FLAG(flags, USORT_OPT_IGNORE_CASE)) {
            ucol_setStrength(ucol, UCOL_SECONDARY);
        }
    }

    return ucol;
}

static UBool ucol_sorter_equals(const void *object, uint64_t flags)
{
    UErrorCode status;

    status = U_ZERO_ERROR;
    if ((flags & USORT_OPT_SORT_MASK) > USORT_OPT_DEFAULT_SORT) {
        return FALSE;
    }
    if (HAS_FLAG(flags, USORT_OPT_IGNORE_CASE) && UCOL_SECONDARY != ucol_getStrength((const UCollator *) object)) {
        return FALSE;
    }
    if (HAS_FLAG(flags, USORT_OPT_NUM_SORT) && UCOL_ON != ucol_getAttribute((const UCollator *) object, UCOL_NUMERIC_COLLATION, &status)) {
        return FALSE;
    }

    return TRUE;
}

static RBKey *ucol_sorter_keygen(const void *object, const UChar *source, int32_t sourceLength, error_t **UNUSED(error))
{
    uint8_t *key;
    RBKey* result;
    int32_t key_len;
    const UCollator *ucol;

    ucol = (const UCollator *) object;
    key_len = ucol_getSortKey(ucol, source, sourceLength, NULL, 0);
    key = mem_new_n(*key, key_len/* + 1*/);
    ensure(key_len == ucol_getSortKey(ucol, source, sourceLength, key, key_len));
//     key[key_len] = 0;
    result = mem_new(*result);
    result->key = key;
    result->key_len = key_len;

//     return key;
    return result;
}

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static UBool ip_sorter_equals(const void *UNUSED(object), uint64_t flags)
{
    return HAS_FLAG(flags, USORT_OPT_IP_ADDRESS_SORT);
}

// NOTE: faster (?) IP "hashing", with a numeric sort, you get the same result
static RBKey *ip_sorter_keygen(const void *UNUSED(object), const UChar *source, int32_t sourceLength, error_t **UNUSED(error))
{
    RBKey *result;
    struct in6_addr addr6;
    char buffer[INET6_ADDRSTRLEN + 1];

    if (sourceLength >= STR_SIZE(buffer)) {
        memset(&addr6, 0, sizeof(addr6)); // error, treat it as a "zero address"
    } else {
        u_austrncpy(buffer, source, STR_LEN(buffer));
        buffer[STR_LEN(buffer)] = '\0';
        if (1 == inet_pton(AF_INET, buffer, &addr6)) {
            // ok
        } else if (1 == inet_pton(AF_INET6, buffer, &addr6)) {
            // ok
        } else {
            memset(&addr6, 0, sizeof(addr6)); // error, treat it as a "zero address"
        }
    }
    result = mem_new(*result);
    result->key = mem_new(addr6);
    result->key_len = sizeof(addr6);
    memcpy(result->key, &addr6, sizeof(addr6));

    return result;
}

static Sorter sorters[] = {
    { USORT_OPT_DEFAULT_SORT | USORT_OPT_NUM_SORT , ucol_sorter_create, ucol_sorter_equals, ucol_sorter_keygen },
    { USORT_OPT_MONTH_SORT, month_sorter_create, month_sorter_equals, month_sorter_keygen },
    { USORT_OPT_IP_ADDRESS_SORT, NULL, ip_sorter_equals, ip_sorter_keygen }
};

/* ========== helpers ========== */

static int field_cmp(void const *a, void const *b)
{
    int diff;
    USortField *f1 = *(USortField * const *) a;
    USortField *f2 = *(USortField * const *) b;

    if (0 == (diff = (f1->start_field - f2->start_field))) {
        return f1->start_offset - f2->start_offset;
    } else {
        return diff;
    }
}

static void usort_print(const void *k, void *v)
{
    int i, count;
    UString *ustr;

/*
    ustr = (UString *) k;
    if (NULL == v) {
        count = 1;
    } else {
        count = *((int *) v);
    }
    for (i = 0; i < count; i++) {
        u_fputs(ustr->ptr, ustdout);
    }
*/
    ustr = (UString *) v;
    u_fputs(ustr->ptr, ustdout);
}

#ifdef DEBUG
static void print_fields(void)
{
    size_t i;

    for (i = 0; i < dptrarray_length(fields); i++) {
        fprintf(
            stderr,
            "order : %d, start_field = %d, end_field = %d, start_offset = %d, end_offset = %d, options = %lu\n",
            machine_ordered_fields[i]->order,
            machine_ordered_fields[i]->start_field,
            machine_ordered_fields[i]->end_field,
            machine_ordered_fields[i]->start_offset,
            machine_ordered_fields[i]->end_offset,
            machine_ordered_fields[i]->options
        );
    }
}
#endif

#if 0
static void usort_toustring(UString *ustr, const void *key, void *value)
{
    int count;
    UString *kstr;

    count = 1;
    if (NULL != value) {
        count =  *(int *) value;
    }
    kstr = (UString *) key;
    ustring_sprintf(ustr, ">%.*S< (%d) => %d", kstr->len, kstr->ptr, kstr->len, count);
}
#endif

static int parse_option(int opt, uint64_t *flags, error_t **error)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(sortnames); i++) {
        if (sortnames[i].short_opt_val == opt) {
            if (0 != (*flags & USORT_OPT_SORT_MASK)) {
                size_t j;

                for (j = 0; j < ARRAY_SIZE(sortnames); j++) {
                    if ((*flags & USORT_OPT_SORT_MASK) == sortnames[j].flag_value) {
                        error_set(error, FATAL, "option '%c' (%s sort) is incompatible with previous option '%c' (%s sort)", opt, sortnames[i].name, sortnames[j].short_opt_val, sortnames[j].name);
                    }
                }
                return FALSE;
            } else {
                *flags |= sortnames[i].flag_value;
                return TRUE;
            }
        }
    }
    error_set(error, FATAL, "invalid option '%c'", opt);

    return FALSE;
}

static UBool create_sorter(USortField *field, error_t **error)
{
    size_t i;
    uint64_t flags;

    flags = field->options & USORT_OPT_SORT_MASK;
    // <find better>
    if (0 == flags) {
        field->options |= USORT_OPT_DEFAULT_SORT;
        flags = field->options & USORT_OPT_SORT_MASK;
    }
    // </find better>
    // if not, create one
    if (NULL == field->sorter) {
        for (i = 0; i < ARRAY_SIZE(sorters); i++) {
            if (HAS_FLAG(sorters[i].implemented, flags)) {
                field->sorter = &sorters[i];
                if (NULL != field->sorter->create) {
                    field->private = field->sorter->create(field->options, error);
                }
                if (NULL != *error) {
                    return FALSE;
                }
            }
        }
    }
    // fields.sorter is still NULL here: oops, not implemented
    if (NULL == field->sorter) {
        for (i = 0; i < ARRAY_SIZE(sortnames); i++) {
            if (sortnames[i].flag_value == field->options) {
                error_set(error, FATAL, "%s (%c) is not (yet) implemented", sortnames[i].name, sortnames[i].short_opt_val);
                return FALSE;
            }
        }
    }

    return TRUE;
}

// \d+(.\d+)?[bdfgiMhnRrV]*(,\d+(.\d+)?[bdfgiMhnRrV]*)?
static UBool parse_field(const char *string, error_t **error)
{
    UBool ret;
    int32_t min;
    char *endptr;
    const char *p;
    ParseNumError err;
    USortField field = INIT_USORT_FIELD;

    min = 1;
    p = string;
    if (
        PARSE_NUM_NO_ERR != (err = parse_int32_t(p, &endptr, 10, &min, NULL, &field.start_field)) // ^\d+$
        && (PARSE_NUM_ERR_NON_DIGIT_FOUND != err)                                                 // ^\d+
    ) {
        error_set(error, FATAL, "%s:\n%s\n%*c", "incorrect value for start field", string, endptr - string + 1, '^');
        return FALSE;
    }
    --field.start_field;
    if ('.' == *endptr) {
        p = endptr + 1;
        if (
            PARSE_NUM_NO_ERR != (err = parse_int32_t(p, &endptr, 10, &min, NULL, &field.start_offset)) // ^\d+\.\d+$
            && PARSE_NUM_ERR_NON_DIGIT_FOUND != err                                                    // ^\d+\.\d+.*
        ) {
            error_set(error, FATAL, "%s:\n%s\n%*c", "incorrect value for start offset (of start field)", string, endptr - string + 1, '^');
            return FALSE;
        }
        --field.start_offset;
    }
    p = endptr;
    while ('\0' != *p && ',' != *p) { // check .* is in fact [bdfgiMhnRrV]*
        if (!parse_option(*p, &field.options, error)) {
//             error_set(error, FATAL, "invalid option '%c'", *p);
            return FALSE;
        }
        ++p;
    }
    if (',' == *p) {
        if (
            PARSE_NUM_NO_ERR != (err = parse_int32_t(++p, &endptr, 10, &min, NULL, &field.end_field)) // ,\d+$
            && (PARSE_NUM_ERR_NON_DIGIT_FOUND != err && '.' != *endptr)                               // ,\d+\.
        ) {
            error_set(error, FATAL, "%s:\n%s\n%*c", "incorrect value for end field", string, endptr - string + 1, '^');
            return FALSE;
        }
        p = endptr;
        --field.end_field;
        if ('.' == *endptr) {
            if (
                PARSE_NUM_NO_ERR != (err = parse_int32_t(++p, &endptr, 10, &min, NULL, &field.end_offset)) // ,\d+\.\d+$
                && PARSE_NUM_ERR_NON_DIGIT_FOUND != err                                                    // ,\d+\.\d+.*$
            ) {
                error_set(error, FATAL, "%s:\n%s\n%*c", "incorrect value for end offset (of end field)", string, endptr - string + 1, '^');
                return FALSE;
            }
            --field.end_offset;
        }
        p = endptr;
        while ('\0' != *p) { // check .* is in fact [bdfgiMhnRrV]*
            if (!parse_option(*p, &field.options, error)) {
//                 error_set(error, FATAL, "invalid option '%c'", *p);
                return FALSE;
            }
            ++p;
        }
    }
    if ('\0' != *p) {
        error_set(error, FATAL, "extra characters found:\n%s\n%*c", string, p - string + 1, '^');
        return FALSE;
    }
    field.order = dptrarray_length(fields);
    if (0 == field.options) {
        field.options = global_options;
    }
    {
        size_t i;
        USortField *f;
        uint64_t flags;

        flags = field.options & USORT_OPT_SORT_MASK;
//         if (0 == flags) {
//             field.options |= USORT_OPT_DEFAULT_SORT;
//         }
        // share a sorter if a compatible one has already been created
        for (i = 0; i < dptrarray_length(fields); i++) {
            f = dptrarray_at_unsafe(fields, i, USortField);
            if (f->sorter->equals(f->private, flags)) {
                field.sorter = f->sorter;
                field.private = f->private;
            }
        }
#if 0
        // if not, create one
        if (NULL == field.sorter) {
            for (i = 0; i < ARRAY_SIZE(sorters); i++) {
                if (sorters[i].implemented == flags) {
                    field.sorter = &sorters[i];
                    field.private = field.sorter->create(field.options, error);
                    if (NULL != *error) {
                        return FALSE;
                    }
                }
            }
        }
        // fields.sorter is still NULL here: oops, not implemented
        if (NULL == field.sorter) {
            for (i = 0; i < ARRAY_SIZE(sortnames); i++) {
                if (sortnames[i].flag_value == field.options) {
                    error_set(error, FATAL, "%s (%c) is not (yet) implemented", sortnames[i].name, sortnames[i].short_opt_val);
                    return FALSE;
                }
            }
        }
#else
        ret = create_sorter(&field, error);
#endif
    }
    dptrarray_push(fields, &field);

    return ret;
}

#if 0
static int32_t split_on_indices(error_t **error, UBreakIterator *ubrk, UString *ustr, DArray *array, interval_list_t *intervals)
{
    dlist_element_t *el;
    int32_t pieces, l, u, lastU;

    lastU = pieces = l = u = 0;
    if (NULL == ubrk) {
        /*for (el = intervals->head; NULL != el && (size_t) u < ustr->len; el = el->next) {
            FETCH_DATA(el->data, i, interval_t);

            if (i->lower_limit > 0) {
                U16_FWD_N(ustr->ptr, l, ustr->len, i->lower_limit - lastU);
                u = l;
            }
            U16_FWD_N(ustr->ptr, u, ustr->len, i->upper_limit - i->lower_limit);
            add_match(array, ustr, l, u);
            ++pieces;
            lastU = i->upper_limit;
            l = u;
        }*/
    } else {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ubrk_setText(ubrk, ustr->ptr, ustr->len, &status);
        if (U_FAILURE(status)) {
            icu_error_set(error, FATAL, status, "ubrk_setText");
            return -1;
        }
        if (UBRK_DONE != (l = ubrk_first(ubrk))) {
            /*for (el = intervals->head; NULL != el && UBRK_DONE != u; el = el->next) {
                FETCH_DATA(el->data, i, interval_t);

                if (i->lower_limit > 0) {
                    if (!ubrk_fwd_n(ubrk, i->lower_limit - lastU, &l)) {
                        break;
                    }
                }
                if (!ubrk_fwd_n(ubrk, i->upper_limit - i->lower_limit, &u)) {
                    break;
                }
                add_match(array, ustr, l, u);
                ++pieces;
                lastU = i->upper_limit;
                l = u;
            }*/
        }
        if (UBRK_DONE != l && UBRK_DONE == u && (size_t) l < ustr->len) {
            add_match(array, ustr, l, ustr->len);
            ++pieces;
        }
        ubrk_setText(ubrk, NULL, 0, &status);
        assert(U_SUCCESS(status));
    }

    return pieces;
}
#endif

/*
echo -en "1\n10\n12\n100\n101\n1" | ./usort
echo -en "janvier\nfévrier\nmars\navril" | ./usort --sort=month
echo -en "0\tjanvier\n0\tfévrier\n0\tmars\n0\tavril" | ./usort --sort=month -k 2
echo -en "0Xjanvier\n0Xfévrier\n0Xmars\n0Xavril" | ./usort -t 'X' -r --sort=month -k 2
*/
static int procfile(reader_t *reader, const char *filename)
{
    RBKey *key;
    error_t *error;

    key = NULL;
    error = NULL;
    if (reader_open(reader, &error, filename)) {
        while (!reader_eof(reader)) {
            if (!reader_readline(reader, &error, ustr)) {
                print_error(error);
            }
            ustring_chomp(ustr);
            if (bFlag) {
                ustring_ltrim(ustr);
            }
#if 1 /* split here */
{
            darray_clear(pieces);
            if (pdata.engine->split(&error, pdata.pattern, ustr, pieces, NULL)) {
                size_t i, count;

                count = darray_length(pieces);
                for (i = 0; i < dptrarray_length(fields); i++) {
                    match_t m;
                    USortField *field;

                    field = dptrarray_at_unsafe(fields, i, USortField);
assert(field->start_field <= count);
                    m = darray_at_unsafe(pieces, field->start_field, match_t);
                    if (field->start_field != field->end_field) {
                        // ...
                    }
                    key = field->sorter->keygen(field->private, m.ptr, m.len, &error);
                    if (NULL != error) {
                        if (NULL != key) {
                            rbkey_destroy(key);
                        }
                        break; // error is handled right after
                    }
#ifdef DEBUG
                    {
                        int i, j;
                        char *dump;

                        dump = mem_new_n(*dump, STR_LEN("0xXX") * key->key_len + 1);
                        for (i = j = 0; i < key->key_len; i++, j += STR_LEN("0xXX")) {
                            sprintf(dump + j, "0x%02X", key->key[i]);
                        }
                        dump[j] = '\0';
                        debug("For >%S<, sort on >%.*S< (%d) >%S<: key = >%s<", ustr->ptr, m.len, m.ptr, m.len, m.ptr, dump);
                        free(dump);
                    }
#endif /* DEBUG */
                }
            } else {
                break; // error is handled right after
            }
// pdata.engine->split(&error, pdata.pattern, ustr, pieces, intervals) // fields
// count = split_on_indices(&error, ubrk, ustr, pieces, intervals) // "caractères" d'un field
}
#endif
            // TODO: put a Line struct as value
            if (/**/TRUE || /**/uFlag) {
                rbtree_insert(tree, key, ustr, RBTREE_INSERT_ON_DUP_KEY_PRESERVE, NULL);
            } else {
                int v;
                int *p;

                v = 1;
                if (!rbtree_insert(tree, key, (void *) &v, RBTREE_INSERT_ON_DUP_KEY_FETCH, (void **) &p)) {
                    (*p)++;
                }
            }
        }
    }
    reader_close(reader);
    if (NULL != error) {
        print_error(error);
        return 1;
    }

    return 0;
}

/* ========== main ========== */

int main(int argc, char **argv)
{
    int wanted;
    int c, ret;
    error_t *error;
    reader_t *reader;
    UErrorCode status;
    const char *separator_arg;

    ret = 0;
    wanted = ALL;
    error = NULL;
    separator_arg = NULL;
    env_init(USORT_EXIT_FAILURE);
    reader = reader_new(DEFAULT_READER_NAME);

    status = U_ZERO_ERROR;
    fields = dptrarray_new(SIZE_TO_DUP_T(sizeof(USortField)), free);
    env_register_resource(fields, (func_dtor_t) dptrarray_destroy);
    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'b':
                bFlag = TRUE;
                global_options |= USORT_OPT_IGNORE_LEAD_BLANKS;
                break;
            case 'f':
                global_options |= USORT_OPT_IGNORE_CASE;
                break;
            case 'k':
                if (!parse_field(optarg, &error)) {
                    print_error(error);
//                     return EXIT_FAILURE;
                }
                break;
//             case 'n':
//                 global_options |= USORT_OPT_NUM_SORT;
//                 break;
            case 'r':
                cmp_func = ucol_key_cmp_r;
                break;
            case 't':
                separator_arg = optarg;
                break;
            case 'u':
                uFlag = TRUE;
//                 global_options |= ?;
                break;
            case VERSION_OPT:
                fprintf(stderr, "BSD %s version %u.%u\n" COPYRIGHT, __progname, UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                return EXIT_SUCCESS;
            case MIN_OPT:
                wanted = MIN_ONLY;
                break;
            case MAX_OPT:
                wanted = MAX_ONLY;
                break;
            case 'g':
            case 'h':
            case 'i':
            case 'M':
            case 'n':
            case 'R':
            case 'V':
                if (!parse_option(c, &global_options, &error)) {
                    print_error(error);
                }
                break;
            case SORT_OPT:
            {
                size_t i;
                UBool found;

                for (i = 0, found = FALSE; !found && i < ARRAY_SIZE(sortnames); i++) {
                    if (0 == strcmp(sortnames[i].name, optarg)) {
                        found = TRUE;
                        global_options |= sortnames[i].flag_value;
                    }
                }
                if (!found) {
                    usage();
                }
                break;
            }
            default:
                if (!util_opt_parse(c, optarg, reader)) {
                    usage();
                }
                break;
        }
    }
    argc -= optind;
    argv += optind;

    env_apply();

#if 1
    if (NULL == separator_arg) {
        separator = ustring_dup_string_len(DEFAULT_SEPARATOR, STR_LEN(DEFAULT_SEPARATOR));
    } else {
        if (NULL == (separator = ustring_convert_argv_from_local(separator_arg, &error, TRUE))) {
            print_error(error);
        }
    }
    if (0 == dptrarray_length(fields)) {
        USortField default_field = INIT_USORT_FIELD;

        default_field.options = global_options;
//         if (0 == (default_field.options & USORT_OPT_SORT_MASK)) {
//             default_field.options |= USORT_OPT_DEFAULT_SORT;
//         }
        // TODO: check options
        if (!create_sorter(&default_field, &error)) {
            print_error(error);
        }
        dptrarray_push(fields, &default_field);
    }
# if 0
    for (i = 0; i < dptrarray_length(fields); i++) {
        USortField *field;

        field = dptrarray_at_unsafe(fields, i, USortField);
        if (NULL == sorter_from_flags(field->options, &error)) {
            print_error(error);
        }
    }
# endif
    machine_ordered_fields = dptrarray_to_array(fields, FALSE, FALSE);
    env_register_resource(machine_ordered_fields, free);
# ifdef DEBUG
    print_fields();
# endif
    if (dptrarray_length(fields) > 1) {
        qsort(machine_ordered_fields, dptrarray_length(fields), sizeof(*machine_ordered_fields), field_cmp);
# ifdef DEBUG
        printf("----\n");
        print_fields();
# endif
    }
    if (NULL == (pdata.pattern = pdata.engine->compile(&error, separator, 0))) {
        print_error(error);
    } else {
        env_register_resource(pdata.pattern, pdata.engine->destroy);
    }
    pieces = darray_new(sizeof(match_t));
    env_register_resource(pieces, (func_dtor_t) darray_destroy);
#endif
//     tree = rbtree_collated_new(ucol, rFlag, (dup_t) ustring_dup, uFlag ? NODUP : SIZE_TO_DUP_T(sizeof(int)), (func_dtor_t) ustring_destroy, uFlag ? NULL : free);
    tree = rbtree_new(cmp_func, NODUP /* key duper */, (dup_t) ustring_dup /*value duper */, (func_dtor_t) rbkey_destroy/*free*/ /* key dtor */, (func_dtor_t) ustring_destroy /* value dtor */);
    env_register_resource(tree, (func_dtor_t) rbtree_destroy);
    ustr = ustring_new();
    env_register_resource(ustr, (func_dtor_t) ustring_destroy);

    if (0 == argc) {
        ret |= procfile(reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            ret |= procfile(reader, *argv);
        }
    }

    if (!rbtree_empty(tree)) {
        switch (wanted) {
            case MIN_ONLY:
            {
                void *k;

                rbtree_min(tree, &k, NULL);
                usort_print(k, NULL);
                break;
            }
            case MAX_ONLY:
            {
                void *k;

                rbtree_max(tree, &k, NULL);
                usort_print(k, NULL);
                break;
            }
            default:
            {
                rbtree_traverse(tree, IN_ORDER, usort_print);
            }
        }
    }

    return (0 == ret ? USORT_EXIT_SUCCESS : USORT_EXIT_FAILURE);
}
