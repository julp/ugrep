#include <unistd.h>
#ifdef _MSC_VER
# define STRICT
# include <windows.h>
# include <psapi.h>
# pragma comment(lib,"Psapi.lib")
char __progname[_MAX_PATH] = "<unknown>";
#endif /* _MSC_VER */

#include "common.h"

#ifdef _MSC_VER
INITIALIZER_DECL(ustdio_init);
#endif /* _MSC_VER */

UFILE *ustdout = NULL;
UFILE *ustderr = NULL;

#ifdef DEBUG
int verbosity = INFO;
#else
int verbosity = WARN;
#endif /* DEBUG */

int exit_failure_value = 0;

UBool stdout_is_tty(void)
{
    return (isatty(STDOUT_FILENO));
}

UChar *local_to_uchar(const char *cargv, int32_t *uargv_length, error_t **error)
{
    UChar *uargv;
    UConverter *ucnv;
    UErrorCode status;
    int32_t cargv_length;
    int32_t _uargv_length;
    int32_t allocated;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(NULL, &status);
    if (U_FAILURE(status)) {
        icu_error_set(error, FATAL, status, "ucnv_open");
        return NULL;
    }
    cargv_length = strlen(cargv);
    allocated = cargv_length * ucnv_getMaxCharSize(ucnv);
    uargv = mem_new_n(*uargv, allocated + 1);
    _uargv_length = ucnv_toUChars(ucnv, uargv, allocated, cargv, cargv_length, &status);
    ucnv_close(ucnv);
    if (U_FAILURE(status)) {
        free(uargv);
        icu_error_set(error, FATAL, status, "ucnv_toUChars");
        return NULL;
    }
    uargv[_uargv_length] = 0;
    if (NULL != uargv_length) {
        *uargv_length = _uargv_length;
    }

    return uargv;
}

UChar32 *local_to_uchar32(const char *cargv, int32_t *u32argv_length, error_t **error)
{
    UChar *u16argv;
    UChar32 *u32argv;
    UErrorCode status;
    int32_t u16argv_length;
    int32_t _u32argv_length;

    if (NULL == (u16argv = local_to_uchar(cargv, &u16argv_length, error))) {
        return NULL;
    } else {
        status = U_ZERO_ERROR;
        _u32argv_length = u_countChar32(u16argv, u16argv_length);
        u32argv = mem_new_n(*u32argv, _u32argv_length + 1);
        u_strToUTF32(u32argv, _u32argv_length, NULL, u16argv, u16argv_length, &status);
        if (U_FAILURE(status)) {
            free(u16argv);
            free(u32argv);
        }
        //u32argv[_u32argv_length] = 0;
        free(u16argv);
        if (NULL != u32argv_length) {
            *u32argv_length = _u32argv_length;
        }
        return u32argv;
    }
}

void print_error(error_t *error)
{
    if (NULL != error && error->type >= verbosity) {
        int type;

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
        u_fputs(error->message, ustderr);
        error_destroy(error);
        if (type == FATAL) {
            exit(exit_failure_value);
        }
    }
}

void report(int type, const char *format, ...)
{
    if (type >= verbosity) {
        va_list args;

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
        u_vfprintf(ustderr, format, args);
        va_end(args);
        if (type == FATAL) {
            exit(exit_failure_value);
        }
    }
}

enum {
    TRIM_LEFT  = 1,
    TRIM_RIGHT = 2,
    TRIM_BOTH  = 3
};

static int32_t _u_trim(
    UChar *string, int32_t string_length,
    UChar *what, int32_t what_length,
    int mode
) {
    int32_t i, k;
    UChar32 c = 0;
    int32_t start = 0, end;
    int32_t string_cu_length, what_cu_length;

    if (string_length < 0) {
        string_cu_length = u_strlen(string);
    } else {
        string_cu_length = string_length;
    }
    if (NULL != what) {
        if (0 == *what) {
            what = NULL;
        } else if (what_length < 0) {
            what_cu_length = u_strlen(what);
        } else {
            what_cu_length = what_length;
        }
    }
    end = string_cu_length;
    if (mode & TRIM_LEFT) {
        for (i = k = 0 ; i < end ; ) {
            U16_NEXT(string, k, end, c);
            if (NULL != what) {
                if (NULL == u_memchr32(what, c, what_cu_length)) {
                    break;
                }
            } else {
                if (FALSE == u_isWhitespace(c)) {
                    break;
                }
            }
            i = k;
        }
        start = i;
    }
    if (mode & TRIM_RIGHT) {
        for (i = k = end ; i > start ; ) {
            U16_PREV(string, 0, k, c);
            if (NULL != what) {
                if (NULL == u_memchr32(what, c, what_cu_length)) {
                    break;
                }
            } else {
                if (FALSE == u_isWhitespace(c)) {
                    break;
                }
            }
            i = k;
        }
        end = i;
    }
    if (start < string_cu_length) {
        u_memmove(string, string + start, end - start);
        *(string + end - start) = 0;
    } else {
        *string = 0;
    }

    return end - start;
}

int32_t u_trim(UChar *s, int32_t s_length, UChar *what, int32_t what_length)
{
    return _u_trim(s, s_length, what, what_length, TRIM_BOTH);
}

int32_t u_ltrim(UChar *s, int32_t s_length, UChar *what, int32_t what_length)
{
    return _u_trim(s, s_length, what, what_length, TRIM_LEFT);
}

int32_t u_rtrim(UChar *s, int32_t s_length, UChar *what, int32_t what_length)
{
    return _u_trim(s, s_length, what, what_length, TRIM_RIGHT);
}

INITIALIZER_P(ustdio_init)
{
#ifdef _MSC_VER
    GetModuleBaseNameA(GetCurrentProcess(), NULL, __progname,  sizeof(__progname)/sizeof(char));
    if (stdout_is_tty()) {
        char cp[30] = "";

        snprintf(cp, sizeof(cp), "CP%d", GetConsoleOutputCP());
        ustdout = u_finit(stdout, NULL, cp);
        ustderr = u_finit(stderr, NULL, cp);
        /**
         * /!\ Don't use ustdout or ustderr before following lines (it includes debug macro) /!\
         **/
        ustdout = u_finit(stdout, NULL, cp);
        ustderr = u_finit(stderr, NULL, cp);
    } else
#endif /* _MSC_VER */
    {
        /**
         * /!\ Don't use ustdout or ustderr before following lines (it includes debug macro) /!\
         **/
        ustdout = u_finit(stdout, NULL, NULL);
        ustderr = u_finit(stderr, NULL, NULL);
    }
    {
        UErrorCode status;

        status = U_ZERO_ERROR;
        ucnv_setSubstChars(u_fgetConverter(ustdout), "?", 1, &status);
        if (U_FAILURE(status)) {
            icu_msg(FATAL, status, "ucnv_setSubstChars");
        }
    }

    debug("system locale = " YELLOW("%s"), u_fgetlocale(ustdout));
    debug("system codepage = " YELLOW("%s"), u_fgetcodepage(ustdout));
}
