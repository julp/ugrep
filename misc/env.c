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

static const char *system_encoding = NULL;
static const char *inputs_encoding = NULL;
static const char *outputs_encoding = NULL;
static const char *stdin_encoding = NULL;
static UNormalizationMode normalization = UNORM_NFD;

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
    {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ucnv_setSubstChars(u_fgetConverter(ustdout), "?", 1, &status);
        if (U_FAILURE(status)) {
            icu_msg(FATAL, status, "ucnv_setSubstChars");
        }
    }
    ustderr = u_finit(stderr, NULL, outputs_encoding);
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

void env_init(void)
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
}
