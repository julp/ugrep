#include <unistd.h>
#ifdef _MSC_VER
# define STRICT
# include <windows.h>
# include <psapi.h>
# pragma comment(lib,"Psapi.lib")
char __progname[_MAX_PATH] = "<unknown>";
#endif /* _MSC_VER */

#include "common.h"

UFILE *ustdout = NULL;
UFILE *ustderr = NULL;

/**
 * 1 inputs in general
 * 2 outputs (stdout/stderr)
 * 3 stdin as special input case (if absent, inherits from 1 if !stdin_is_tty, 2 if stdin_is_tty, else default)
 **/

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

// inputs/outputs
static const char *system_encoding = NULL;
static const char *inputs_encoding = NULL;
static const char *outputs_encoding = NULL;
static const char *stdin_encoding = NULL;
// unicode stuffs
static int unit = UNIT_CODEPOINT;
static UNormalizationMode normalization = UNORM_NONE;//UNORM_NFC;
// error handling
#ifdef DEBUG
static int verbosity = INFO;
#else
static int verbosity = WARN;
#endif /* DEBUG */
static int exit_failure_value = 0;

void env_set_verbosity(int type)
{
    switch (type) {
        case INFO:
        case WARN:
        case FATAL:
            verbosity = type;
            break;
        default:
            fprintf(stderr, "Unknown error level (%d), skip\n", type);
    }
}

void print_error(error_t *error)
{
    if (NULL != error && error->type >= verbosity) {
        int type;
        UFILE *ustderrp;

        if (NULL == (ustderrp = ustderr)) {
            ustderrp = u_finit(stderr, NULL, NULL);
        }
        type = error->type;
        switch (type) {
            case WARN:
                u_fprintf(ustderr, "[ " YELLOW("WARN") " ] ");
                break;
            case FATAL:
                u_fprintf(ustderr, "[ " RED("ERR ") " ] ");
                break;
            default:
                type = FATAL;
                u_fprintf(ustderr, "[ " RED("BUG ") " ] Unknown error type for:\n");
                break;
        }
        u_fputs(error->message, ustderrp);
        if (NULL == ustderr) {
            u_fclose(ustderrp);
        }
        error_destroy(error);
        if (FATAL == type) {
            exit(exit_failure_value);
        }
    }
}

void report(int type, const char *format, ...)
{
    if (type >= verbosity) {
        va_list args;
        UFILE *ustderrp;

        if (NULL == (ustderrp = ustderr)) {
            ustderrp = u_finit(stderr, NULL, NULL);
        }
        switch (type) {
            case INFO:
                fprintf(stderr, "[ " GREEN("INFO") " ] ");
                break;
            case WARN:
                fprintf(stderr, "[ " YELLOW("WARN") " ] ");
                break;
            case FATAL:
                fprintf(stderr, "[ " RED("ERR ") " ] ");
                break;
        }
        va_start(args, format);
        u_vfprintf(ustderrp, format, args);
        va_end(args);
        if (NULL == ustderr) {
            u_fclose(ustderrp);
        }
        if (FATAL == type) {
            exit(exit_failure_value);
        }
    }
}

int env_get_unit(void)
{
    return unit;
}

void env_set_unit(int mode)
{
    switch (unit) {
        case UNIT_GRAPHEME:
        case UNIT_CODEPOINT:
            unit = mode;
            break;
        default:
            fprintf(stderr, "Unknown unit (%d), skip\n", mode);
    }
}

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
    UErrorCode status;

    status = U_ZERO_ERROR;
    if (NULL != system_encoding) {
        ucnv_setDefaultName(system_encoding);
    }
    ustderr = u_finit(stderr, NULL, outputs_encoding);
    env_register_resource(ustderr, (func_dtor_t) u_fclose);
//     u_fsetlocale(ustderr, outputs_encoding);
    ucnv_setSubstChars(u_fgetConverter(ustderr), "?", 1, &status);
    if (U_FAILURE(status)) {
        icu_msg(FATAL, status, "ucnv_setSubstChars");
    }
    ustdout = u_finit(stdout, NULL, outputs_encoding);
    env_register_resource(ustdout, (func_dtor_t) u_fclose);
    ucnv_setSubstChars(u_fgetConverter(ustdout), "?", 1, &status);
    if (U_FAILURE(status)) {
        icu_msg(FATAL, status, "ucnv_setSubstChars");
    }
    if (NULL == stdin_encoding) {
        if (stdin_is_tty()) {
            stdin_encoding = outputs_encoding;
        } else {
            stdin_encoding = inputs_encoding;
        }
    }
    if (UNIT_GRAPHEME == unit && UNORM_NONE == normalization) {
        report(INFO, "Working at grapheme level implies a normalization for consistency. Switch on NFC normalization.\n");
        normalization = UNORM_NFC;
    }
    debug("system encoding = " YELLOW("%s"), ucnv_getDefaultName());
    debug("outputs encoding = " YELLOW("%s"), u_fgetcodepage(ustdout));
}

void env_init(int failure_value)
{
    const char *tmp;

    exit_failure_value = failure_value;
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
    GetModuleBaseNameA(GetCurrentProcess(), NULL, __progname,  ARRAY_SIZE(__progname));
    if (NULL == outputs_encoding && stdout_is_tty()) {
        char cp[30] = { 0 };

        snprintf(cp, ARRAY_SIZE(cp), "CP%d", GetConsoleOutputCP());
        outputs_encoding = mem_dup(cp);
        env_register_resource(outputs_encoding, free);
    }
#endif /* _MSC_VER */
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

void env_unregister_resource(void *ptr) /* NONNULL() */
{
    resource_t *current, *prev;

    prev = NULL;
    current = resources;
    while (NULL != current) {
        if (current->ptr == ptr) {
            if (NULL == prev) {
                resources = current->next;
            } else {
                prev->next = current->next;
            }
            current->dtor_func(current->ptr);
            free(current);
            break;
        }
#if defined(DEBUG) && 0
        fprintf(stderr, "freeing %p, registered in %s at line %d\n", current->ptr, current->filename, current->lineno);
#endif /* DEBUG */
        prev = current;
        current = current->next;
    }
}

#include <unicode/uclean.h>
void env_close(void)
{
    if (NULL != resources) {
        resource_t *current, *next;

        current = resources;
        while (NULL != current) {
            next = current->next;
#if defined(DEBUG) && 0
            fprintf(stderr, "freeing %p, registered in %s at line %d\n", current->ptr, current->filename, current->lineno);
#endif /* DEBUG */
            current->dtor_func(current->ptr);
            free(current);
            current = next;
        }
        resources = NULL;
    }
    u_cleanup();
}
