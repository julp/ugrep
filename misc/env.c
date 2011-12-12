#include <unistd.h>
#ifdef _MSC_VER
# define STRICT
# include <windows.h>
# include <psapi.h>
# pragma comment(lib,"Psapi.lib")
char __progname[_MAX_PATH] = "<unknown>";
#endif /* _MSC_VER */

#include <unicode/ures.h>
#include <unicode/umsg.h>

#include "common.h"

UFILE *ustdout = NULL;
UFILE *ustderr = NULL;

/**
 * 1 inputs in general
 * 2 outputs (stdout/stderr)
 * 3 stdin as special input case (if absent, inherits from 1 if !stdin_is_tty, 2 if stdin_is_tty, else default)
 **/

static const char *system_encoding = NULL;
static const char *inputs_encoding = NULL;
static const char *outputs_encoding = NULL;
static const char *stdin_encoding = NULL;
static UNormalizationMode normalization = UNORM_NONE;//UNORM_NFC;

static UResourceBundle *ures = NULL;

/**
 *                          SYSTEM
 *                          /    \
 *                         /     \
 *                        /      \
 * outputs (stdout and stderr)   inputs in general
 *                       /        \
 *                      /         \
 *                     /          \
 *    if(stdin_is_tty) \          / if(!stdin_is_tty)
 *                     \         /
 *                     \        /
 *                     \       /
 *                     \      /
 *                     \     /
 *                      stdin
 **/

UNormalizationMode env_get_normalization(void)
{
    return normalization;
}

void env_set_normalization(UNormalizationMode mode)
{
    switch (mode) {
        case UNORM_NONE:
        case UNORM_NFD:
        case UNORM_NFC:
            normalization = mode;
            break;
        case UNORM_NFKD:
        case UNORM_NFKC:
            fprintf(stderr, "Compatibility (de)composition rejected (%d), skip\n", mode);
            break;
        default:
            fprintf(stderr, "Unknown (de)composition (%d), skip\n", mode);
    }
}

static UBool env_check_encoding(const char *encoding)
{
    UConverter *ucnv;
    UErrorCode status;

    ucnv = NULL;
    status = U_ZERO_ERROR;
    ucnv_open(encoding, &status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    ucnv_close(ucnv);
    return TRUE;
}

void env_set_system_encoding(const char *encoding)
{
    if (env_check_encoding(encoding)) {
        system_encoding = encoding;
    } else {
        fprintf(stderr, "invalid systeme encoding '%s', skip\n", encoding);
    }
}

const char *env_get_inputs_encoding(void)
{
    return inputs_encoding;
}

void env_set_inputs_encoding(const char *encoding)
{
    if (env_check_encoding(encoding)) {
        inputs_encoding = encoding;
    } else {
        fprintf(stderr, "invalid encoding '%s' for inputs, skip\n", encoding);
    }
}

void env_set_outputs_encoding(const char *encoding)
{
    if (env_check_encoding(encoding)) {
        outputs_encoding = encoding;
    } else {
        fprintf(stderr, "invalid encoding '%s' for outputs, skip\n", encoding);
    }
}

const char *env_get_stdin_encoding(void)
{
    return inputs_encoding;
}

void env_set_stdin_encoding(const char *encoding)
{
    if (env_check_encoding(encoding)) {
        stdin_encoding = encoding;
    } else {
        fprintf(stderr, "invalid encoding '%s' for stdin, skip\n", encoding);
    }
}

void env_apply(void)
{
    if (NULL != system_encoding) {
        ucnv_setDefaultName(system_encoding);
    }
    ustdout = u_finit(stdout, NULL, outputs_encoding);
    env_register_resource(ustdout, (func_dtor_t) u_fclose);
    {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ucnv_setSubstChars(u_fgetConverter(ustdout), "?", 1, &status);
        if (U_FAILURE(status)) {
            icu_msg(FATAL, status, "ucnv_setSubstChars");
        }
    }
    ustderr = u_finit(stderr, NULL, outputs_encoding);
    env_register_resource(ustderr, (func_dtor_t) u_fclose);
    {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ucnv_setSubstChars(u_fgetConverter(ustderr), "?", 1, &status);
        if (U_FAILURE(status)) {
            icu_msg(FATAL, status, "ucnv_setSubstChars");
        }
    }
    if (NULL == stdin_encoding) {
        if (stdin_is_tty()) {
            stdin_encoding = outputs_encoding;
        } else {
            stdin_encoding = inputs_encoding;
        }
    }
#ifdef DEBUG
    debug("system encoding = " YELLOW("%s"), ucnv_getDefaultName());
    debug("outputs encoding = " YELLOW("%s"), u_fgetcodepage(ustdout));
#endif /* DEBUG */
}

static char *binary_path(const char *, char *, size_t);

void env_init(const char *argv0)
{
    const char *tmp;

#ifdef BSD
{
#include <sys/types.h>
#include <pwd.h>
#include <login_cap.h>

    login_cap_t *lc;
    struct passwd *pwd;

    if (NULL != (pwd = getpwuid(getuid()))) {
        if (NULL != (lc = login_getuserclass(pwd))) {
            if (NULL != (tmp = login_getcapstr(lc, "charset", NULL, NULL))) {
                env_set_system_encoding(tmp);
            }
            login_close(lc);
        } else {
            if (NULL != (lc = login_getpwclass(pwd))) {
                if (NULL != (tmp = login_getcapstr(lc, "charset", NULL, NULL))) {
                    env_set_system_encoding(tmp);
                }
                login_close(lc);
            }
        }
    }
    if (NULL != (tmp = getenv("MM_CHARSET"))) {
        env_set_system_encoding(tmp);
    }
}
#endif /* BSD */
    if (NULL != (tmp = getenv("UGREP_SYSTEM"))) {
        env_set_system_encoding(tmp);
    }
    if (NULL != (tmp = getenv("UGREP_OUTPUT"))) {
        env_set_outputs_encoding(tmp);
    }
#ifdef _MSC_VER
    GetModuleBaseNameA(GetCurrentProcess(), NULL, __progname,  sizeof(__progname)/sizeof(char));
    if (NULL == outputs_encoding && stdout_is_tty()) {
        char cp[30] = { 0 };

        snprintf(cp, sizeof(cp), "CP%d", GetConsoleOutputCP());
        outputs_encoding = strdup(cp); // TODO: leak
    }
#endif /* _MSC_VER */
#ifndef WITHOUT_NLS
    {
        char rbpath[MAXPATHLEN];

        strcpy(rbpath, "/home/julp/ugrep/ugrep");
//         *rbpath = '\0';
# ifdef _MSC_VER
        //
# else
        /*{ WRONG
# include <errno.h>

            char bin[MAXPATHLEN];

            if (NULL == realpath(argv0, bin)) {
                stdio_debug("realpath failed: %s", strerror(errno));
            } else {
                char *c;

                if (NULL != (c = strrchr(bin, DIRECTORY_SEPARATOR))) {
                    c[1] = '\0';
                    strncat(bin, "../share/", STR_SIZE(bin));
                    if (NULL == realpath(bin, rbpath)) {
                        *rbpath = '\0';
                        stdio_debug("realpath failed: %s", strerror(errno));
                    } else {
                        strncat(rbpath, "/ugrep/ugrep", STR_SIZE(bin));
                    }
                }
            }
        }*/
        /*{
# include <errno.h>
            int ret;
            char proc[MAXPATHLEN];

            ret = snprintf(proc, STR_SIZE(proc), "/proc/%d/exe", getpid());
            if (ret < 0 || ret >= (int) STR_SIZE(proc)) {
                *rbpath = '\0';
            } else {
                ssize_t fill;

                if (-1 == (fill = readlink(proc, rbpath, STR_SIZE(rbpath)))) {
                    *rbpath = '\0';
                    stdio_debug("readlink failed: %s", strerror(errno));
                } else {
                    rbpath[fill] = '\0';
                    strncat(rbpath, "/ugrep/ugrep", STR_SIZE(rbpath));
                }
            }
        }*/
# endif /* _MSC_VER */
        {
            char binary[MAXPATHLEN];

            binary_path(argv0, binary, MAXPATHLEN); // TODO: return value: test/leak
        }
        if ('\0' != *rbpath) {
            UErrorCode status;

            status = U_ZERO_ERROR;
stdio_debug("%s", rbpath);
            ures = ures_open(rbpath, NULL, &status);
            if (U_FAILURE(status)) {
                fprintf(stderr, "translation disabled: %s\n", u_errorName(status));
# ifdef DEBUG
            } else {
                env_register_resource(ures, (func_dtor_t) ures_close);
                if (U_USING_DEFAULT_WARNING == status) {
                    fprintf(stderr, YELLOW("default") " translation enabled\n");
                } else {
                    fprintf(stderr, "translation enabled: " GREEN("%s") "\n", ures_getLocaleByType(ures, ULOC_ACTUAL_LOCALE, &status));
                    assert(U_SUCCESS(status));
                }
# endif /* DEBUG */
            }
        }
    }
#endif /* !WITHOUT_NLS */
    if (0 != atexit(env_close)) {
        fputs("can't register atexit() callback", stderr);
        exit(EXIT_FAILURE);
    }
}

typedef struct resource_t {
    void *ptr;
    func_dtor_t dtor_func;
    struct resource_t *next;
#ifdef DEBUG
    int lineno;
    const char *filename;
#endif /* DEBUG */
} resource_t;

static resource_t *resources = NULL; /* LIFO */

#ifdef DEBUG
void _env_register_resource(void *ptr, func_dtor_t dtor_func, const char *filename, int lineno) /* NONNULL() */
#else
void env_register_resource(void *ptr, func_dtor_t dtor_func) /* NONNULL() */
#endif /* DEBUG */
{
    resource_t *res;

    require_else_return(NULL != ptr);
    require_else_return(NULL != dtor_func);

    res = mem_new(*res);
    res->next = resources;
    res->ptr = ptr;
    res->dtor_func = dtor_func;
#ifdef DEBUG
    res->filename = filename; // no dup
    res->lineno = lineno;
#endif /* DEBUG */
    resources = res;
}

#include <unicode/uclean.h>
void env_close(void)
{
    if (NULL != resources) {
        resource_t *current, *next;

        current = resources;
        while (NULL != current) {
            next = current->next;
//             if (NULL != current->ptr && NULL != current->dtor_func) {
                current->dtor_func(current->ptr);
//             }
            free(current);
            current = next;
        }
        resources = NULL;
    }
    u_cleanup();
}

#define URESOURCE_SEPARATOR '/'

static char **str_split(char separator, const char *string)
{
    int count, i;
    const char *p;
    char *dup, **pieces;

    p = string;
    i = count = 0;
    if (NULL == (dup = strdup(string))) {
        return NULL;
    }
    for (p = string; '\0' != *p; p++) {
        if (*p == separator) {
            ++count;
        }
    }
    pieces = mem_new_n(*pieces, count + 2);
    pieces[i++] = dup;
    while ('\0' != *dup) {
        if (separator == *dup) {
            *dup = '\0';
            pieces[i++] = dup + 1;
        }
        ++dup;
    }
    pieces[i++] = NULL;

    return pieces;
}

// we don't use readlink("/proc/%d/exe", getpid()) which is not portable
static char *binary_path(const char *argv0, char *buffer, size_t buffer_size)
{
    size_t argv0_len;
    char *retval, resolved[MAXPATHLEN];

    retval = NULL;
    argv0_len = strlen(argv0);
    if ('.' == *argv0 || NULL != strchr(argv0, '/')) { // path is relative or (seems) absolute
        if (NULL != realpath(argv0, resolved)) {
            if (NULL == buffer) {
                retval = strdup(resolved);
            } else {
                size_t resolved_len;

                resolved_len = strlen(resolved);
                if (buffer_size > resolved_len) {
                    strcpy(buffer, resolved);
                    retval = buffer;
                }
            }
        }
    } else { // look in PATH
        size_t p_len;
        const char *PATH;
        char **paths, **p;
        char fullpath[MAXPATHLEN];

        if (NULL == (PATH = getenv("PATH"))) {
            return NULL;
        }
        if (NULL == (paths = str_split(':', PATH))) {
            return NULL;
        }
        for (p = paths; NULL != *p; p++) {
            p_len = strlen(*p);
            if (p_len + STR_LEN("/") + argv0_len > sizeof(fullpath) - 1) {
                break;
            }
            memcpy(fullpath, *p, p_len);
            fullpath[p_len] = '/';
            memcpy(fullpath + p_len + STR_LEN("/"), argv0, argv0_len);
            fullpath[p_len + STR_LEN("/") + argv0_len] = '\0';
            if (0 == access(fullpath, X_OK)) {
                if (NULL != realpath(fullpath, resolved)) {
                    if (NULL == buffer) {
                        retval = strdup(resolved);
                    } else {
                        size_t resolved_len;

                        resolved_len = strlen(resolved);
                        if (buffer_size > resolved_len) {
                            strcpy(buffer, resolved);
                            retval = buffer;
                        }
                    }
                }
                break;
            }
        }
        free(paths[0]);
        free(paths);
    }

stdio_debug("resolved : %s", retval);
    return retval;
}

#ifndef WITHOUT_NLS
// _("icu", u_errorName(status), u_errorName(status))
// _("ucut", "encodingIs", "encoding is: %s")
// ns can be NULL, fallback too to indicate to use id?
UChar *_(const char *UNUSED(ns), const char *id, const char *fallback)
{
    UErrorCode status;
    int32_t msg_len, result_len;
    UChar *result, *msg, buf[256];

    status = U_ZERO_ERROR;
#if 0
    if (NULL != ures) {
        msg = (UChar *) ures_getStringByKey(ures, id, &msg_len, &status);
        if (U_SUCCESS(status)) {
            return msg;
        }
    }
#else
    if (NULL != ures) {
        char *p;
        int subres;
        UResourceBundle *to, *from;

        to = ures;
        if (URESOURCE_SEPARATOR == *id) {
            // p = id + 1 or invalid
        }
        subres = 0;
        for (p = (char *) id; '\0' != *p; p++) {
            if (URESOURCE_SEPARATOR == *p) {
                ++subres;
            }
        }
        if (subres > 0) {
            int i, j;
            char *dup, **names;

            i = 0;
            dup = strdup(id);
            names = mem_new_n(*names, subres + 2);
            names[i++] = dup;
            for (p = dup; '\0' != *p; p++) {
                if (URESOURCE_SEPARATOR == *p) {
                    *p = '\0';
                    names[i++] = p + 1;
                }
            }
            names[i++] = NULL;
            from = ures;
            to = NULL;
debug("subres = %d", subres);
            for (j = 0; j < subres/*NULL != names[i]*/; j++) {
debug("%s", names[j]);
                to = ures_getByKey(from, names[j], to, &status);
                if (U_FAILURE(status)) {
                    debug("%s", u_errorName(status));
                    goto failed;
                }
                from = to;
                //assert(U_SUCCESS(status));
            }
debug("%s", names[j]);
            msg = (UChar *) ures_getStringByKey(to, names[j], &msg_len, &status);
            free(names[0]);
            free(names);
            if (NULL != to) {
                ures_close(to);
            }
            /*if (ures != from) { // don't close twice the same UResourceBundle
                ures_close(from);
            }*/
        } else {
            msg = (UChar *) ures_getStringByKey(ures, id, &msg_len, &status);
        }
        if (U_SUCCESS(status)) {
            return msg;
        }
    }
failed:
#endif
    u_uastrncpy(buf, fallback, STR_LEN(buf));
    result_len = u_strlen(buf);
    result = mem_new_n(*result, result_len + 1);
    u_memcpy(result, buf, result_len);
    result[result_len] = 0;
    //env_register_resource(result, free);

    return result;
}
#endif /* !WITHOUT_NLS */
