#include <unistd.h>

#include "common.h"

#ifdef WITH_FTS
# include <errno.h>
# include <libgen.h>
# include <fts.h>
# include <fnmatch.h>
# include "struct/slist.h"

static int dirbehave = DIR_READ;
static int linkbehave = LINK_READ;
static int devbehave = DEV_READ;

static slist_t *directory_patterns = NULL;
static slist_t *file_patterns = NULL;
static UBool have_directory_excluded = FALSE;
static UBool have_directory_included = FALSE;
static UBool have_file_excluded = FALSE;
static UBool have_file_included = FALSE;

int get_dirbehave(void)
{
    return dirbehave;
}

typedef struct {
    char *pattern;
    int mode;
} FTSPattern;

static void fts_pattern_free(void *data)
{
    FTSPattern *p = (FTSPattern *) data;

    free(p->pattern);
}

static void add_fts_pattern(const char *pattern, slist_t **l, int mode)
{
    FTSPattern *p;

    if (NULL == *l) {
        *l = slist_new(fts_pattern_free);
        env_register_resource(*l, (func_dtor_t) slist_destroy);
    }
    p = mem_new(*p);
    p->pattern = strdup(pattern); // TODO: NULL checking
    p->mode = mode;
    slist_append(*l, p);
}

UBool is_file_matching(char *fname)
{
    UBool ret;
    char *fname_base;
    slist_element_t *e;

    if (!have_file_included && !have_file_excluded) {
        return TRUE;
    }

    ret = have_file_included ? FALSE : TRUE;
    fname_base = basename(fname);
    for (e = file_patterns->head; NULL != e; e = e->next) {
        FETCH_DATA(e->data, p, FTSPattern);

        if (fnmatch(p->pattern, fname, 0) == 0 || fnmatch(p->pattern, fname_base, 0) == 0) {
            if (FTS_EXCLUDE == p->mode) {
                return FALSE;
            } else {
                ret = TRUE;
            }
        }
    }

    return ret;
}

static inline UBool is_directory_matching(const char *dname)
{
    UBool ret;
    slist_element_t *e;

    ret = have_directory_included ? FALSE : TRUE;
    for (e = directory_patterns->head; NULL != e; e = e->next) {
        FETCH_DATA(e->data, p, FTSPattern);

        if (NULL != dname && fnmatch(p->pattern, dname, 0) == 0) {
            if (FTS_EXCLUDE == p->mode) {
                return FALSE;
            } else {
                ret = TRUE;
            }
        }
    }

    return ret;
}

int procdir(reader_t *reader, char **argv, void *userdata, int (*procfile)(reader_t *reader, const char *filename, void *userdata))
{
    int ret;
    FTS *fts;
    FTSENT *p;
    int ftsflags;

    ret = 0;
    switch(linkbehave) {
        case LINK_EXPLICIT:
            ftsflags = FTS_COMFOLLOW;
            break;
        case LINK_SKIP:
            ftsflags = FTS_PHYSICAL;
            break;
        default:
            ftsflags = FTS_LOGICAL;
    }
    ftsflags |= FTS_NOSTAT | FTS_NOCHDIR;
    if (NULL == (fts = fts_open(argv, ftsflags, NULL))) {
        msg(FATAL, "can't fts_open %s: %s", *dirname, strerror(errno));
    }
    while (NULL != (p = fts_read(fts))) {
        switch (p->fts_info) {
            case FTS_DNR:
            case FTS_ERR:
                msg(WARN, "fts_read failed on %s: %s", p->fts_path, strerror(p->fts_errno));
                break;
            case FTS_D:
            case FTS_DP:
                if (have_directory_excluded || have_directory_included) {
                    if (!is_directory_matching(p->fts_name) || !is_directory_matching(p->fts_path)) {
                        fts_set(fts, p, FTS_SKIP);
                    }
                }
                break;
            case FTS_DC:
                msg(WARN, "recursive directory loop on %s", p->fts_path);
                break;
            default:
            {
                UBool ok;

                ok = TRUE;
                if (have_file_excluded || have_file_included) {
                    ok &= is_file_matching(p->fts_path);
                }
                if (ok) {
                    ret |= procfile(reader, p->fts_path, userdata);
                }
                break;
            }
        }
    }
    fts_close(fts);

    return ret;
}
#endif /* WITH_FTS */

UBool util_opt_parse(int c, const char *optarg, reader_t *reader)
{
    switch (c) {
        case FORM_OPT:
            if (!strcasecmp("none", optarg)) {
                env_set_normalization(UNORM_NONE);
                return TRUE;
            } else if (!strcasecmp("c", optarg)) {
                env_set_normalization(UNORM_NFC);
                return TRUE;
            } else if (!strcasecmp("d", optarg)) {
                env_set_normalization(UNORM_NFD);
                return TRUE;
            }
            return FALSE;
        case UNIT_OPT:
            if (!strcmp("codepoint", optarg) || !strcmp("cp", optarg)) {
                env_set_unit(UNIT_CODEPOINT);
                return TRUE;
            } else if (!strcmp("grapheme", optarg)) {
                env_set_unit(UNIT_GRAPHEME);
                return TRUE;
            }
            return FALSE;
        case READER_OPT:
            if (!reader_set_imp_by_name(reader, optarg)) {
                fprintf(stderr, "Unknown reader\n");
                return FALSE;
            }
            return TRUE;
        case INPUT_OPT:
            env_set_inputs_encoding(optarg);
            return TRUE;
        case STDIN_OPT:
            env_set_stdin_encoding(optarg);
            return TRUE;
        case OUTPUT_OPT:
            env_set_outputs_encoding(optarg);
            return TRUE;
        case SYSTEM_OPT:
            env_set_system_encoding(optarg);
            return TRUE;
#ifdef WITH_FTS
        case 'D':
            if (!strcasecmp(optarg, "skip")) {
                devbehave = DEV_SKIP;
            } else if (!strcasecmp(optarg, "read")) {
                devbehave = DEV_READ;
            } else {
                fprintf(stderr, "Invalid --devices option\n");
                return FALSE;
            }
            return TRUE;
        case 'd':
            if (!strcasecmp("recurse", optarg)) {
                dirbehave = DIR_RECURSE;
            } else if (!strcasecmp("skip", optarg)) {
                dirbehave = DIR_SKIP;
            } else if (!strcasecmp("read", optarg)) {
                dirbehave = DIR_READ;
            } else {
                fprintf(stderr, "Invalid --directories option\n");
                return FALSE;
            }
            return TRUE;
        case 'r':
        case 'R':
            dirbehave = DIR_RECURSE;
            return TRUE;
        case 'p':
            linkbehave = LINK_SKIP;
            return TRUE;
        case 'O':
            linkbehave = LINK_EXPLICIT;
            return TRUE;
        case 'S':
            linkbehave = LINK_READ;
            return TRUE;
        case FTS_INCLUDE_DIR_OPT:
            have_directory_included = TRUE;
            add_fts_pattern(optarg, &directory_patterns, FTS_INCLUDE);
            return TRUE;
        case FTS_EXCLUDE_DIR_OPT:
            have_directory_excluded = TRUE;
            add_fts_pattern(optarg, &directory_patterns, FTS_EXCLUDE);
            return TRUE;
        case FTS_INCLUDE_FILE_OPT:
            have_file_included = TRUE;
            add_fts_pattern(optarg, &file_patterns, FTS_INCLUDE);
            return TRUE;
        case FTS_EXCLUDE_FILE_OPT:
            have_file_excluded = TRUE;
            add_fts_pattern(optarg, &file_patterns, FTS_EXCLUDE);
            return TRUE;
#endif /* WITH_FTS */
        default:
            return FALSE;
    }
}

UBool stdout_is_tty(void)
{
    return (isatty(STDOUT_FILENO));
}

UBool stdin_is_tty(void)
{
    return (isatty(STDIN_FILENO));
}

void ubrk_unbindText(UBreakIterator *ubrk)
{
    UErrorCode status;

    assert(NULL != ubrk);
    status = U_ZERO_ERROR;
    ubrk_setText(ubrk, NULL, 0, &status);
    assert(U_SUCCESS(status));
}

static const UChar _UREGEXP_FAKE_USTR[] = { 0 };
#define UREGEXP_FAKE_USTR _UREGEXP_FAKE_USTR, 0

void uregex_unbindText(URegularExpression *uregex)
{
    UErrorCode status;

    assert(NULL != uregex);
    status = U_ZERO_ERROR;
    uregex_setText(uregex, UREGEXP_FAKE_USTR, &status);
    assert(U_SUCCESS(status));
}

static UChar _USEARCH_FAKE_USTR[] = { 0, 0 };
#define USEARCH_FAKE_USTR _USEARCH_FAKE_USTR, 1

void usearch_unbindText(UStringSearch *usearch)
{
    UErrorCode status;

    assert(NULL != usearch);
    status = U_ZERO_ERROR;
    usearch_setText(usearch, USEARCH_FAKE_USTR, &status);
    assert(U_SUCCESS(status));
}
