#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <errno.h>

#include <unicode/uset.h>
#include <unicode/ubrk.h>

#include "common.h"
#include "struct/hashtable.h"

enum {
    UTR_EXIT_SUCCESS = 0,
    UTR_EXIT_FAILURE,
    UTR_EXIT_USAGE
};

enum {
    GRAPHEME_MODE,
    CODE_POINT_MODE
};

#define DEFAULT_MODE GRAPHEME_MODE

enum {
    NONE,
    CHARACTER,       // a single code point, idea would be to optimize against STRING (more than one code point)
    STRING,
    SET,
    FILTER_FUNCTION,
    SIMPLE_FUNCTION, // simple translation (case mapping)
    GLOBAL_FUNCTION, // full translation (case mapping)
    COUNT
} /* set_type_t*/;

/*
+----------+----------+----+------+
| set1     | set2     | -c | note |
+----------+----------+----+------+
| function | none     | ko | ok (apply this translation function) => special case without -d, others are forbidden
+----------+----------+----+-------
| char     | char     | ok | strtr
| char     | string   | -- | forbidden
| char     | class    | -- | impossible
| char     | function | ok | ok => no sense ?
+----------+----------+----+-------
| string   | char     | ok | strtr
| string   | string   | ko | strtr
| string   | class    | -- | impossible
| string   | function | ok | ok => no sense ?
+----------+----------+----+-------
| set      | char     | ok | ok
| set      | string   | -- | forbidden
| set      | class    | -- | impossible
| set      | function | ok | ok => no sense ?
+----------+----------+----+-------
| function | char     | ok | ok
| function | string   | -- | forbidden
| function | class    | -- | impossible
| function | function | ok | ok => no sense ?
+----------+----------+----+-------

char: UChar32
string: UChar32 *
class: UChar *
function: - (stay with char *)
*/

typedef UBool (*filter_func_t)(UChar32);
typedef UChar32 (*simple_translate_func_t)(UChar32);

typedef struct {
    const char *name;
    filter_func_t func;
} filter_func_decl_t;

typedef struct {
    const char *name;
    UCaseType ct;
} case_map_element_t;

/* ========== global variables ========== */

filter_func_decl_t filter_functions[] = {
    {"isupper",         u_isupper},
    {"islower",         u_islower},
    {"istitle",         u_istitle},
    {"isdigit",         u_isdigit},
    {"isalpha",         u_isalpha},
    {"isalnum",         u_isalnum},
    {"isxdigit",        u_isxdigit},
    {"ispunct",         u_ispunct},
    {"isgraph",         u_isgraph},
    {"isblank",         u_isblank},
    {"isdefined",       u_isdefined},
    {"isspace",         u_isspace},
    {"isJavaSpaceChar", u_isJavaSpaceChar},
    {"isWhitespace",    u_isWhitespace},
    {"iscntrl",         u_iscntrl},
    {"isISOControl",    u_isISOControl},
    {"isprint",         u_isprint},
    {"isbase",          u_isbase},
    {NULL,              NULL}
};

case_map_element_t case_map[] = {
    {"toupper", UCASE_UPPER},
    {"tolower", UCASE_LOWER},
    {"totitle", UCASE_TITLE},
    {NULL,      UCASE_NONE}
};

static const simple_translate_func_t simple_case_mapping[UCASE_COUNT] = {
    NULL,      // UCASE_NONE
    NULL,      // UCASE_FOLD
    u_tolower,
    u_toupper,
    u_totitle
};

/* ========== getopt stuff ========== */

static char optstr[] = "Ccdstv";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"complement",      no_argument,       NULL, 'c'},
    {"delete",          no_argument,       NULL, 'd'},
    {"squeeze-repeats", no_argument,       NULL, 's'},
    {"truncate",        no_argument,       NULL, 't'},
    {"version",         no_argument,       NULL, 'v'},
    {NULL,              no_argument,       NULL, 0}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s [-%s] SET1 [SET2] [STRING]\n",
        __progname,
        optstr
    );
    exit(UTR_EXIT_USAGE);
}

/* ========== set helpers ========== */

USet *create_set_from_argv(const char *cpattern, UBool negate, error_t **error)
{
    USet *uset;
    UChar *upattern;
    UErrorCode status;
    int32_t upattern_length;

    status = U_ZERO_ERROR;
    if (NULL == (upattern = local_to_uchar(cpattern, &upattern_length, error))) {
        return NULL;
    }
    uset = uset_openPattern(upattern, upattern_length, &status);
    if (U_FAILURE(status)) {
        free(upattern);
        icu_error_set(error, FATAL, status, "uset_openPattern");
        return NULL;
    }
    free(upattern);
    if (negate) {
        uset_complement(uset);
    }
    uset_freeze(uset);

    return uset;
}

USet *create_set_from_string32(const UChar32 *string32, int32_t string32_length, UBool negate, error_t **UNUSED(error))
{
    USet *uset;
    int i;

    uset = uset_openEmpty();
    for (i = 0; i < string32_length; i++) {
        uset_add(uset, string32[i]);
    }
    if (negate) {
        uset_complement(uset);
    }
    uset_freeze(uset);

    return uset;
}

/* ========== X ========== */

int32_t grapheme_count(UBreakIterator *ubrk, const UChar *ustring, int32_t ustring_len)
{
    int32_t i, count;
    UErrorCode status;

    count = 0;
    status = U_ZERO_ERROR;
    ubrk_setText(ubrk, ustring, ustring_len, &status);
    if (U_FAILURE(status)) {
        return -1;
    }
    for (i = ubrk_first(ubrk); UBRK_DONE != i; i = ubrk_next(ubrk)) {
        ++count;
    }
    ubrk_setText(ubrk, NULL, 0, &status);
    assert(U_SUCCESS(status));

    return count;
}

/* ========== X ========== */

#define POINTER_TO_UCHAR32(p) ((UChar32) (long) (p))
#define TO_POINTER(c) ((void *) (long) (c))

typedef struct {
    UChar *ptr;
    size_t len;
} KVString;

void *kvstring_dup(const void *v)
{
    KVString *s, *clone;

    s = (KVString *) v;
    clone = mem_new(*clone);
    clone->ptr = s->ptr; // we don't need to dup it too
    clone->len = s->len;

    return clone;
}

#ifdef DEBUG
static const UChar NULL_USTRING[] = { 0x28, 0x6E, 0x75, 0x6C, 0x6C, 0x29, 0 };

static const KVString NULL_KVString = {
    &NULL_USTRING,
    6
};

void kvstring_debug(UString *output, const void *k, const void *v)
{
    const KVString *kk, *kv;

    kk = (const KVString *) k;
    kv = NULL == v ? &NULL_KVString : (const KVString *) v;
    ustring_sprintf(output, "key = >%.*S< (%d), value = >%.*S< (%d)", kk->len, kk->ptr, kk->len, kv->len, kv->ptr, kv->len);
}
#endif /* DEBUG */

int single_equal(const void *a, const void *b)
{
    const KVString *s1, *s2;

    s1 = (const KVString *) a;
    s2 = (const KVString *) b;

    return 0 == u_strCompare(s1->ptr, s1->len, s2->ptr, s2->len, FALSE);
}

uint32_t single_hash(const void *k)
{
    size_t i;
    uint32_t h;
    const KVString *s;

    h = 0;
    s = (const KVString *) k;
    for (i = 0; i < s->len; i++) {
        h = h * 31 + s->ptr[i];
    }

    return h;
}

/*
// not needed if based on UChar * instead of UChar32
int cp_equal(const void *a, const void *b)
{
    return POINTER_TO_UCHAR32(a) == POINTER_TO_UCHAR32(b);
}

uint32_t cp_hash(const void *k)
{
    return k;
}*/

/* ========== X ========== */

Hashtable *grapheme_hashtable_put(
    UChar *from, int32_t from_length,
    UChar *to, int32_t to_length, // NULL, 0 if -d
    UBool delete, UBool complete
) {
    Hashtable *ht;
    KVString k, v;
    UErrorCode status;
    int32_t l1, u1;
    UBreakIterator *ubrk1, *ubrk2;

    ht = hashtable_new(single_hash, single_equal, NULL, NULL, kvstring_dup, delete ? NULL : kvstring_dup);
    status = U_ZERO_ERROR;
    ubrk1 = ubrk_open(UBRK_CHARACTER, NULL, from, from_length, &status);
    if (U_FAILURE(status)) {
        // error
        return NULL;
    }
    if (delete) {
        if (UBRK_DONE != (l1 = ubrk_first(ubrk1))) {
            while (UBRK_DONE != (u1 = ubrk_next(ubrk1))) {
                k.ptr = from + l1;
                k.len = u1 - l1;
                hashtable_put(ht, &k, NULL);

                l1 = u1;
            }
        }
    } else {
        int32_t l2, u2;

        ubrk2 = ubrk_open(UBRK_CHARACTER, NULL, to, to_length, &status);
        if (U_FAILURE(status)) {
            // error
            ubrk_close(ubrk1);
            return NULL;
        }
        if (UBRK_DONE != (l1 = ubrk_first(ubrk1)) && UBRK_DONE != (l2 = ubrk_first(ubrk2))) {
            while (UBRK_DONE != (u1 = ubrk_next(ubrk1)) && UBRK_DONE != (u2 = ubrk_next(ubrk2))) {
                k.ptr = from + l1;
                k.len = u1 - l1;
                v.ptr = to + l2;
                v.len = u2 - l2;
                hashtable_put(ht, &k, &v);

                l1 = u1;
                l2 = u2;
            }
            if (UBRK_DONE != u1) { // "hack" (for now) for set2_type == CHARACTER
                u2 = ubrk_last(ubrk2);
                l2 = ubrk_previous(ubrk2);
                do {
                    k.ptr = from + l1;
                    k.len = u1 - l1;
                    v.ptr = to + l2;
                    v.len = u2 - l2;
                    hashtable_put(ht, &k, &v);

                    l1 = u1;
                } while (UBRK_DONE != (u1 = ubrk_next(ubrk1)));
            }
        }
        ubrk_close(ubrk2);
    }
    ubrk_close(ubrk1);

    return ht;
}

#if 0
Hashtable *cp_hashtable_put(
    UChar *from, int32_t from_length,
    UChar *to, int32_t to_length,
    UBool squeeze, UBool delete, UBool complete
) {
    Hashtable *ht;

    ht = hashtable_new(...);
    // ...

    return ht;
}
#endif

/* ========== replacement helpers ========== */

static UBool ustring_endswith(UString *ustr, UChar *str, size_t length)
{
    return ustr->len >= length && 0 == u_memcmp(ustr->ptr + ustr->len - length, str, length);
}

void grapheme_process(
    Hashtable *ht,
    UBreakIterator *ubrk,
    UString *in, UString *out,
    UBool squeeze, UBool delete
) {
    int32_t l, u;
    KVString k, *v;
    UErrorCode status;

    status = U_ZERO_ERROR;
    ubrk_setText(ubrk, in->ptr, in->len, &status);
    if (U_FAILURE(status)) {
        // TODO
        return;
    }
    if (UBRK_DONE != (l = ubrk_first(ubrk))) {
        while (UBRK_DONE != (u = ubrk_next(ubrk))) {
            k.ptr = in->ptr + l;
            k.len = u - l;
            if (delete) {
                if (!hashtable_exists(ht, &k)) {
                    if (!squeeze || !ustring_endswith(out, k.ptr, k.len)) {
                        ustring_append_string_len(out, k.ptr, k.len);
                    }
                }
            } else {
                if (hashtable_get(ht, &k, (void **) &v)) {
                    if (!squeeze || !ustring_endswith(out, v->ptr, v->len)) {
                        ustring_append_string_len(out, v->ptr, v->len);
                    }
                } else {
                    if (!squeeze || !ustring_endswith(out, k.ptr, k.len)) {
                        ustring_append_string_len(out, k.ptr, k.len);
                    }
                }
            }
            l = u;
        }
    }
    ubrk_setText(ubrk, NULL, 0, &status);
    assert(U_SUCCESS(status));
}

#if 0
void cp_process(
    Hashtable *h,
    UChar *in, int32_t in_length,
    UBool squeeze, UBool delete
) {
    UChar32 last = U_SENTINEL;

    //
}

void single_cp_tr(
    UChar *from, int32_t from_length, // UTF-16 (binaire) ou UTF-32 (macros) based ?
    UChar *to, int32_t to_length, // UTF-16 (binaire) ou UTF-32 (macros) based ?
    UChar *in, int32_t in_length,
    UBool squeeze
) {
    UChar32 last = U_SENTINEL; // un UBool suffit dans ce cas particulier ?

    //
}

void single_cp_delete(
    UChar *from, int32_t from_length, // UTF-16 (binaire) ou UTF-32 (macros) based ?
    UChar *in, int32_t in_length,
) {
    //
}
#endif

void trtr(
    UChar32 *from, int32_t from_length,
    UChar32 *to, int32_t to_length,
    UString *in, UString *out
) {
    size_t i;
    int32_t f;
    UChar32 ci;
    UBool isError;

    isError = FALSE; // unused
    for (i = 0; i < in->len; ) {
        U16_NEXT(in->ptr, i, in->len, ci);
        for (f = 0; f < from_length; f++) {
            if (from[f] == ci) {
                if (NULL == to || 0 == *to || 0 == to_length) { // dFlag on
                    goto endinnerloop;
                } else if (1 == to_length) { // dFlag off, to_length == 1 (to is a single code point)
                    ustring_append_char32(out, to[0]);
                    goto endinnerloop;
                } else { // dFlag off, to_length == from_length
                    ustring_append_char32(out, to[f]);
                    goto endinnerloop;
                }
            }
        }
        ustring_append_char32(out, ci);

endinnerloop:
        ;
    }
}

void cptr(
    UChar32 from, UChar32 to,
    UString *in, UString *out,
    UBool negate
) {
    size_t i;
    UChar32 ci;
    UBool isError;

    isError = FALSE; // unused
    for (i = 0; i < in->len; ) {
        U16_NEXT(in->ptr, i, in->len, ci);
        if (((from == ci) ^ negate)) {
            if (-1 != to) {
                ustring_append_char32(out, to);
            }
            continue;
        }
        ustring_append_char32(out, ci);
    }
}

/* ========== main ========== */

int main(int argc, char **argv)
{
    int c;
    USet *uset;
    UBool match;
    UChar32 i32;
    Hashtable *ht;
    error_t *error;
    reader_t reader;
    UChar *from, *to;
    UErrorCode status;
    UString *in, *out;
    UBool set2_expected;
    UBreakIterator *ubrk;
    UChar32 *from32, *to32;
    filter_func_t filter_func;
    int set1_type, set2_type;
    int32_t from_length, to_length;
    UBool dFlag, cFlag, isError, sFlag;
    UCaseType set1_case_type, set2_case_type;

    ht = NULL;
    ubrk = NULL;
    uset = NULL;
    error = NULL;
    in = out = NULL;
    from = to = NULL;
    filter_func = NULL;
    from32 = to32 = NULL;
    status = U_ZERO_ERROR;
    set1_type = set2_type = NONE;
    exit_failure_value = UTR_EXIT_FAILURE;
    set1_case_type = set2_case_type = UCASE_NONE;
    isError = set2_expected = cFlag = dFlag = sFlag = FALSE;

    env_init();
    reader_init(&reader, DEFAULT_READER_NAME);

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'C':
            case 'c':
                cFlag = TRUE;
                break;
            case 'd':
                dFlag = TRUE;
                break;
            case 's':
                sFlag = TRUE;
                break;
            case 't':
                // NOP, ignored, doesn't apply to Unicode
                break;
            case 'v':
                fprintf(stderr, "BSD utr version %u.%u\n" COPYRIGHT, UGREP_VERSION_MAJOR, UGREP_VERSION_MINOR);
                exit(EXIT_SUCCESS);
                break;
            default:
                if (!util_opt_parse(c, optarg, &reader)) {
                    usage();
                }
                break;
        }
    }
    argc -= optind;
    argv += optind;

    env_apply();

    if (dFlag) {
        switch (argc) {
            case 1:
                // set1 (argv[0]) + stdin
                reader_open_stdin(&reader, &error);
                break;
            case 2:
                // set1 (argv[0]) + string (argv[1])
                reader_open_string(&reader, &error, argv[1]);
                break;
            default:
                usage();
                break;
        }
    } else {
        switch (argc) {
            case 1:
                // set1 (argv[0]) + stdin
                reader_open_stdin(&reader, &error);
                break;
            case 2:
                // set1 (argv[0]) as FUNC + string (argv[1])
                // OR
                // set1 (argv[0]) + set2 (argv[1]) + stdin
                if (!strncmp("fn:", argv[0], STR_LEN("fn:"))) {
                    reader_open_string(&reader,  &error, argv[1]);
                } else {
                    set2_expected = TRUE;
                    reader_open_stdin(&reader, &error);
                }
                break;
            case 3:
                // set1 (argv[0]) + set2 (argv[1]) + string (argv[2])
                set2_expected = TRUE;
                reader_open_string(&reader,  &error, argv[2]);
                break;
            default:
                usage();
                break;
        }
        if (set2_expected) {
            if (!strncmp("fn:", argv[1], STR_LEN("fn:"))) {
                case_map_element_t *c;

                for (c = case_map; NULL != c->name; c++) {
                    if (!strcmp(c->name, argv[1] + STR_LEN("fn:"))) {
                        set2_type = SIMPLE_FUNCTION;
                        set2_case_type = c->ct;
                        break;
                    }
                }
                if (UCASE_NONE == set2_case_type) {
                    fprintf(stderr, "Unknown translate function '%s'\n", argv[1] + STR_LEN("fn:"));
                    return UTR_EXIT_USAGE;
                }
            } else {
                if (NULL == (to32 = local_to_uchar32(argv[1], &to_length, &error))) {
                    print_error(error);
                    return UTR_EXIT_FAILURE;
                }
                if (1 == to_length) {
                    set2_type = CHARACTER;
                } else {
                    set2_type = STRING;
                }
            }
        }
    }
    if (!strncmp("fn:", argv[0], STR_LEN("fn:"))) {
        if (NONE == set2_type && !dFlag) {
            case_map_element_t *c;

            for (c = case_map; NULL != c->name; c++) {
                if (!strcmp(c->name, argv[0] + STR_LEN("fn:"))) {
                    set1_type = GLOBAL_FUNCTION;
                    set1_case_type = c->ct;
                    break;
                }
            }
            if (UCASE_NONE == set1_case_type) {
                fprintf(stderr, "Unknown translate function '%s'\n", argv[0] + STR_LEN("fn:"));
                return UTR_EXIT_USAGE;
            }
        } else {
            filter_func_decl_t *f;

            for (f = filter_functions; NULL != f->name; f++) {
                if (!strcmp(f->name, argv[0] + STR_LEN("fn:"))) {
                    set1_type = FILTER_FUNCTION;
                    filter_func = f->func;
                    break;
                }
            }
            if (NULL == filter_func) {
                fprintf(stderr, "Unknown filter function '%s'\n", argv[0] + STR_LEN("fn:"));
                return UTR_EXIT_USAGE;
            }
        }
    } else if ('[' == *argv[0]) {
        set1_type = SET;
        if (NULL == (uset = create_set_from_argv(argv[0], cFlag, &error))) {
            print_error(error);
        }
    } else {
        if (NULL == (from32 = local_to_uchar32(argv[0], &from_length, &error))) {
            print_error(error);
        }
        if (1 == from_length) {
            set1_type = CHARACTER;
        } else {
            if (cFlag) { // readjust
                if (NULL == (uset = create_set_from_string32(from32, from_length, cFlag, &error))) {
                    print_error(error);
                }
                set1_type = SET;
            } else {
                set1_type = STRING;
            }
        }
    }

    if (STRING == set2_type) {
        if (STRING != set1_type) {
            fprintf(stderr, "Using a string as a set have only sense if the first set is defined as a string too\n");
            return UTR_EXIT_FAILURE;
        } else {
            if (cFlag) {
                fprintf(stderr, "Complement option cannot be applied when the both sets are defined as strings\n");
                return UTR_EXIT_FAILURE;
            }
            if (from_length != to_length) {
                fprintf(stderr, "Number of code points differs between sets\n");
                return UTR_EXIT_FAILURE;
            }
        }
    }

    in = ustring_new();
    out = ustring_new();

    if (STRING == set1_type) {
        from = local_to_uchar(argv[0], &from_length, &error);
        if (dFlag) {
            to = NULL;
            to_length = 0;
        } else {
            to = local_to_uchar(argv[1], &to_length, &error);
        }
        ht = grapheme_hashtable_put(from, from_length, to, to_length, dFlag, FALSE);
        //hashtable_debug(ht, kvstring_debug);
        ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
        assert(U_SUCCESS(status));
    }

    while (!reader_eof(&reader)) {
        if (!reader_readline(&reader, &error, in)) {
            print_error(error);
        }
        ustring_chomp(in);
        ustring_truncate(out);

        if (SET == set1_type || FILTER_FUNCTION == set1_type) {
            size_t i;

            for (i = 0; i < in->len; /* none: done by U16_NEXT */) {
                U16_NEXT(in->ptr, i, in->len, i32);
                if (SET == set1_type) {
                    match = uset_contains(uset, i32);
                } else if (FILTER_FUNCTION == set1_type) {
                    match = filter_func(i32) ^ cFlag;
                } /*else { BUG }*/
                if (match) {
                    if (dFlag) {
                        continue;
                    } else if (CHARACTER == set2_type) {
                        i32 = *to32;
                    } else if (SIMPLE_FUNCTION == set2_type) {
                        i32 = simple_case_mapping[set2_case_type](i32);
                    }
                }
                ustring_append_char32(out, i32);
            }
        } else if (CHARACTER == set1_type && CHARACTER == set2_type) {
            cptr(*from32, dFlag ? -1 : *to32, in, out, cFlag);
        } else if (SIMPLE_FUNCTION == set2_type) {
            if (CHARACTER == set1_type) {
                // TODO: ???
                to32 = mem_new(*to32); // we don't use \0 in fact
                to32[0] = from32[0];
                to_length = 1;
            } else if (STRING == set1_type) {
                int i;

                // TODO: ???
                to32 = mem_new_n(*to32, from_length); // we don't use \0 in fact, so forget "+ 1"
                to_length = from_length;
                for (i = 0; i < from_length; i++) {
                    to32[i] = simple_case_mapping[set2_case_type](from32[i]);
                }
            }
            trtr(from32, from_length, to32, to_length, in, out);
        } else if (GLOBAL_FUNCTION == set1_type /*&& set2_type == NONE*/) {
            if (!ustring_fullcase(out, in->ptr, in->len, set1_case_type, &error)) {
                print_error(error);
            }
        } else {
//             trtr(from32, from_length, to32, to_length, in, out);
            grapheme_process(ht, ubrk, in, out, sFlag, dFlag);
        }

        u_file_write(out->ptr, out->len, ustdout);
    }
    reader_close(&reader);

    if (NULL != from) {
        free(from);
    }
    if (NULL != to) {
        free(to);
    }
    if (NULL != from32) {
        free(from32);
    }
    if (NULL != to32) {
        free(to32);
    }
    if (NULL != uset) {
        uset_close(uset);
    }
    if (NULL != ubrk) {
        ubrk_close(ubrk);
    }
    if (NULL != ht) {
        hashtable_destroy(ht);
    }
    if (NULL != in) {
        ustring_destroy(in);
    }
    if (NULL != out) {
        ustring_destroy(out);
    }

    return UTR_EXIT_SUCCESS;
}
