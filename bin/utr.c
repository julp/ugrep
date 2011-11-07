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

enum {
    NONE,
    // SINGLE_CODE_POINT,
    // SINGLE_GRAPHEME,
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

// cpattern is expected to be a class (= surrounded by square brackets)
USet *create_set_from_argv(const char *cpattern, UBool negate, error_t **error)
{
    USet *uset;
    UString *ustr;
    UErrorCode status;

    status = U_ZERO_ERROR;
    if (NULL == (ustr = ustring_convert_argv_from_local(cpattern, error, TRUE))) {
        return NULL;
    }
    uset = uset_openPattern(ustr->ptr, ustr->len, &status);
    if (U_FAILURE(status)) {
        ustring_destroy(ustr);
        icu_error_set(error, FATAL, status, "uset_openPattern");
        return NULL;
    }
    ustring_destroy(ustr);
    if (negate) {
        uset_complement(uset);
    }
    uset_freeze(uset);

    return uset;
}

// pattern (ustr) is not expected to be a class here (no square brackets here)
USet *create_set_from_ustring(UString *ustr, UBool negate, error_t **UNUSED(error))
{
    USet *uset;

    uset = uset_openEmpty();
    uset_addAllCodePoints(uset, ustr->ptr, ustr->len);
    if (negate) {
        uset_complement(uset);
    }
    uset_freeze(uset);

    return uset;
}

/* ========== check lengths ========== */

int32_t grapheme_count(UBreakIterator *ubrk, const UString *ustr)
{
    int32_t i, count;
    UErrorCode status;

    count = 0;
    status = U_ZERO_ERROR;
    ubrk_setText(ubrk, ustr->ptr, ustr->len, &status);
    if (U_FAILURE(status)) {
        return -1;
    }
    if (UBRK_DONE != (i = ubrk_first(ubrk))) {
        while (UBRK_DONE != (i = ubrk_next(ubrk))) {
            ++count;
        }
    }
    ubrk_setText(ubrk, NULL, 0, &status);
    assert(U_SUCCESS(status));

    return count;
}

/* ========== hashtable stuffs (hashing, keys equality, ...) ========== */

typedef struct {
    UChar *ptr;
    size_t len;
} KVString;

#ifdef DEBUG
static const UChar NULL_UCHAR[] = { 0x28, 0x6E, 0x75, 0x6C, 0x6C, 0x29, 0 };

static const KVString NULL_KVString = {
    (UChar *) &NULL_UCHAR,
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

/* ========== hashtable building (parsing command arguments) ========== */

UBool grapheme_hashtable_put(Hashtable *ht, UString *from, UString *to, UBool delete, UBool UNUSED(complete))
{
    KVString k;
    int32_t l1, u1;
    UErrorCode status;
    UBreakIterator *ubrk1;

    ht = hashtable_standalone_dup_new(single_hash, single_equal, sizeof(KVString), delete ? 0 : sizeof(KVString));
    status = U_ZERO_ERROR;
    ubrk1 = ubrk_open(UBRK_CHARACTER, NULL, from->ptr, from->len, &status);
    if (U_FAILURE(status)) {
        // TODO: error
        return FALSE;
    }
    if (delete) {
        if (UBRK_DONE != (l1 = ubrk_first(ubrk1))) {
            while (UBRK_DONE != (u1 = ubrk_next(ubrk1))) {
                k.ptr = from->ptr + l1;
                k.len = u1 - l1;
                hashtable_put(ht, &k, NULL);

                l1 = u1;
            }
        }
    } else {
        KVString v;
        int32_t l2, u2;
        UBreakIterator *ubrk2;

        ubrk2 = ubrk_open(UBRK_CHARACTER, NULL, to->ptr, to->len, &status);
        if (U_FAILURE(status)) {
            // TODO: error
            ubrk_close(ubrk1);
            return FALSE;
        }
        if (UBRK_DONE != (l1 = ubrk_first(ubrk1)) && UBRK_DONE != (l2 = ubrk_first(ubrk2))) {
            while (UBRK_DONE != (u1 = ubrk_next(ubrk1)) && UBRK_DONE != (u2 = ubrk_next(ubrk2))) {
                k.ptr = from->ptr + l1;
                k.len = u1 - l1;
                v.ptr = to->ptr + l2;
                v.len = u2 - l2;
                hashtable_put(ht, &k, &v);

                l1 = u1;
                l2 = u2;
            }
            if (UBRK_DONE != u1) { // "hack" (for now) for set2_type == CHARACTER
                do { // for a while instead of do/while, inverse ubrk_next calls in the while above
                    k.ptr = from->ptr + l1;
                    k.len = u1 - l1;
                    hashtable_put(ht, &k, &v);

                    l1 = u1;
                } while (UBRK_DONE != (u1 = ubrk_next(ubrk1)));
            }
        }
        ubrk_close(ubrk2);
    }
    ubrk_close(ubrk1);

    return TRUE;
}

void cp_hashtable_put(Hashtable *ht, UString *from, UString *to, UBool delete, UBool UNUSED(complete))
{
    UChar32 fc;
    KVString k;
    size_t fi, flast;

    if (delete) {
        flast = fi = 0;
        while (fi < from->len) {
            U16_NEXT(from->ptr, fi, from->len, fc);
            k.ptr = from->ptr + flast;
            k.len = fi - flast;
            hashtable_put(ht, &k, NULL);
            flast = fi;
        }
    } else {
        UChar32 tc;
        KVString v;
        size_t ti, tlast;

        tlast = ti = flast = fi = 0;
        while (fi < from->len && ti < to->len) {
            U16_NEXT(from->ptr, fi, from->len, fc);
            U16_NEXT(to->ptr, ti, to->len, tc);

            k.ptr = from->ptr + flast;
            k.len = fi - flast;
            v.ptr = to->ptr + tlast;
            v.len = ti - tlast;
            hashtable_put(ht, &k, &v);

            flast = fi;
            tlast = ti;
        }
        if (fi < from->len) { // "hack" (for now) for set2_type == CHARACTER
            while (fi < from->len) {
                U16_NEXT(from->ptr, fi, from->len, fc);

                k.ptr = from->ptr + flast;
                k.len = fi - flast;
                hashtable_put(ht, &k, &v);

                flast = fi;
            }
        }
    }
}

/* ========== replacement helpers ========== */

// TODO: merge grapheme_process and cp_process? It would avoid checking in main function
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
        // TODO: error
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

void cp_process(
    Hashtable *ht,
    UString *in, UString *out,
    UBool squeeze, UBool delete
) {
    size_t l, u;
    KVString k, *v;

    l = u = 0;
    while (u < in->len) {
        U16_FWD_1(in->ptr, u, in->len);
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

void single_process(
    UBreakIterator *ubrk, // ubrk = NULL if not in grapheme mode
    UString *from, UString *to, // to = NULL if in delete mode
    UString *in, UString *out,
    UBool squeeze, UBool delete
) {
    debug("UBRK %s", NULL == ubrk ? "none (cp)" : "based (grapheme)");
    if (NULL != ubrk) {
        int32_t l, u;
        UErrorCode status;

        status = U_ZERO_ERROR;
        ubrk_setText(ubrk, in->ptr, in->len, &status);
        if (U_FAILURE(status)) {
            // TODO: error
            return;
        }
        if (UBRK_DONE != (l = ubrk_first(ubrk))) {
            while (UBRK_DONE != (u = ubrk_next(ubrk))) {
                if (0 == u_strCompare(in->ptr + l, u - l, from->ptr, from->len, FALSE)) {
                    if (delete) {
                        /* NOP */
                    } else {
                        if (!squeeze || !ustring_endswith(out, to->ptr, to->len)) {
                            ustring_append_string_len(out, to->ptr, to->len);
                        }
                    }
                } else {
                    if (!squeeze || !ustring_endswith(out, in->ptr + l, u - l)) {
                        ustring_append_string_len(out, in->ptr + l, u - l);
                    }
                }
                l = u;
            }
        }
        ubrk_setText(ubrk, NULL, 0, &status);
        assert(U_SUCCESS(status));
    } else {
        size_t l, u;

        l = u = 0;
        while (u < in->len) {
            U16_FWD_1(in->ptr, u, in->len);
            if (0 == u_strCompare(in->ptr + l, u - l, from->ptr, from->len, FALSE)) {
                if (delete) {
                    /* NOP */
                } else {
                    if (!squeeze || !ustring_endswith(out, to->ptr, to->len)) {
                        ustring_append_string_len(out, to->ptr, to->len);
                    }
                }
            } else {
                if (!squeeze || !ustring_endswith(out, in->ptr + l, u - l)) {
                    ustring_append_string_len(out, in->ptr + l, u - l);
                }
            }
            l = u;
        }
    }
}

/* ========== main ========== */

#define CP_TO_KVString(c, kvs) \
    do { \
        (kvs).len = U16_LENGTH(c); \
        (kvs).ptr = mem_new_n(*(kvs).ptr, (kvs).len); \
        if (((uint32_t) (c)) <= 0xffff) { \
            ((kvs).ptr)[0] = (UChar) (c); \
        } else { \
            ((kvs).ptr)[0] = (UChar) (((c) >> 10) + 0xd7c0); \
            ((kvs).ptr)[1] = (UChar) (((c) & 0x3ff) | 0xdc00); \
        } \
    } while (0);

int main(int argc, char **argv)
{
    int c;
    USet *uset;
    UBool match;
    UChar32 i32;
    Hashtable *ht;
    error_t *error;
    reader_t reader;
    UErrorCode status;
    UBool set2_expected;
    UBreakIterator *ubrk;
    filter_func_t filter_func;
    int set1_type, set2_type;
    UString *in, *out, *set1, *set2;
    UBool dFlag, cFlag, isError, sFlag;
    UCaseType set1_case_type, set2_case_type;

    ht = NULL;
    ubrk = NULL;
    uset = NULL;
    error = NULL;
    filter_func = NULL;
    status = U_ZERO_ERROR;
    set1_type = set2_type = NONE;
    set2 = set1 = in = out = NULL;
    exit_failure_value = UTR_EXIT_FAILURE;
    set1_case_type = set2_case_type = UCASE_NONE;
    match = isError = set2_expected = cFlag = dFlag = sFlag = FALSE;

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
                if (NULL == (set2 = ustring_convert_argv_from_local(argv[1], &error, TRUE))) {
                    print_error(error);
                }
                if (u_strHasMoreChar32Than(set2->ptr, set2->len, 1)) {
                    set2_type = STRING;
                } else {
                    set2_type = CHARACTER;
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
        if (NULL == (set1 = ustring_convert_argv_from_local(argv[0], &error, TRUE))) {
            print_error(error);
        }
        if (!cFlag && UNORM_NONE != env_get_normalization()) {
            ubrk = ubrk_open(UBRK_CHARACTER, NULL, NULL, 0, &status);
            assert(U_SUCCESS(status));
        }
        if ((UNORM_NONE == env_get_normalization() && u_strHasMoreChar32Than(set1->ptr, set1->len, 1)) || (UNORM_NONE != env_get_normalization() && grapheme_count(ubrk, set1) > 1)) {
            if (cFlag) { // readjust
                if (NULL == (uset = create_set_from_ustring(set1, cFlag, &error))) {
                    print_error(error);
                }
                set1_type = SET;
            } else {
                set1_type = STRING;
            }
        } else {
            set1_type = CHARACTER;
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
            if (UNORM_NONE == env_get_normalization() && u_countChar32(set2->ptr, set2->len) != u_countChar32(set1->ptr, set1->len)) {
                fprintf(stderr, "Number of code points differs between sets\n");
                return UTR_EXIT_FAILURE;
            } else if (UNORM_NONE != env_get_normalization() && grapheme_count(ubrk, set2) != grapheme_count(ubrk, set1)) {
                fprintf(stderr, "Number of graphemes differs between sets\n");
                return UTR_EXIT_FAILURE;
            }
        }
    }

    in = ustring_new();
    out = ustring_new();

    if (STRING == set1_type) {
        ht = hashtable_standalone_dup_new(single_hash, single_equal, sizeof(KVString), dFlag ? 0 : sizeof(KVString));
        if (SIMPLE_FUNCTION == set2_type) {
            UChar32 c;
            KVString k, v;
            size_t i, last;

            last = i = 0;
            while (i < set1->len) {
                U16_NEXT(set1->ptr, i, set1->len, c);
                k.ptr = set1->ptr + last;
                k.len = i - last;
                c = simple_case_mapping[set2_case_type](c);
                CP_TO_KVString(c, v);
                hashtable_put(ht, &k, &v);
                last = i;
            }
        } else if (UNORM_NONE == env_get_normalization()) {
            cp_hashtable_put(ht, set1, set2, dFlag, FALSE);
        } else {
            grapheme_hashtable_put(ht, set1, set2, dFlag, FALSE);
        }
    }
#ifdef DEBUG
    {
        const char *typemap[] = {
            "none",
            "single code point or grapheme",
            "string",
            "set",
            "filter function",
            "simple translation function",
            "global translation function"
        };

        debug("mode = %s, set1 = %s, set2 = %s", UNORM_NONE == env_get_normalization() ? "CP/as is" : "graphemes", typemap[set1_type], typemap[set2_type]);
    }
#endif /* DEBUG */
    while (!reader_eof(&reader)) {
        if (!reader_readline(&reader, &error, in)) {
            print_error(error);
        }
        ustring_chomp(in);
        ustring_truncate(out);

        if (SET == set1_type || FILTER_FUNCTION == set1_type) {
            size_t i;

            for (i = 0; i < in->len; /* none: incrementation done by U16_NEXT */) {
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
                        U16_GET_UNSAFE(set2->ptr, 0, i32); // TODO: compute it once, *before* loop
                    } else if (SIMPLE_FUNCTION == set2_type) {
                        i32 = simple_case_mapping[set2_case_type](i32);
                    }
                }
                ustring_append_char32(out, i32);
            }
        } else if (SIMPLE_FUNCTION == set2_type) {
            cp_process(ht, in, out, sFlag, FALSE);
        } else if (CHARACTER == set1_type/* && CHARACTER == set2_type*/) {
            single_process(ubrk, set1, set2, in, out, sFlag, dFlag);
        } else if (GLOBAL_FUNCTION == set1_type/* && set2_type == NONE*/) {
            if (!ustring_fullcase(out, in->ptr, in->len, set1_case_type, &error)) {
                print_error(error);
            }
        } else {
            if (UNORM_NONE == env_get_normalization()) {
                cp_process(ht, in, out, sFlag, dFlag);
            } else {
                grapheme_process(ht, ubrk, in, out, sFlag, dFlag);
            }
        }

        u_file_write(out->ptr, out->len, ustdout);
    }
    reader_close(&reader);

    if (NULL != ubrk) {
        ubrk_close(ubrk);
    }
    if (NULL != uset) {
        uset_close(uset);
    }
    if (NULL != ht) {
        hashtable_destroy(ht);
    }
    if (NULL != set1) {
        ustring_destroy(set1);
    }
    if (NULL != set2) {
        ustring_destroy(set2);
    }
    if (NULL != in) {
        ustring_destroy(in);
    }
    if (NULL != out) {
        ustring_destroy(out);
    }

    return UTR_EXIT_SUCCESS;
}
