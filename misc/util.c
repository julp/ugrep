#include <unistd.h>

#include "common.h"

#ifdef DEBUG
int verbosity = INFO;
#else
int verbosity = WARN;
#endif /* DEBUG */

int exit_failure_value = 0;

UBool util_opt_parse(int c, const char *optarg, reader_t *reader)
{
    switch (c) {
        case NFNONE_OPT:
            env_set_normalization(UNORM_NONE);
            return TRUE;
        case NFD_OPT:
            env_set_normalization(UNORM_NFD);
            return TRUE;
        case NFC_OPT:
            env_set_normalization(UNORM_NFC);
            return TRUE;
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

UChar *local_to_uchar(const char *cargv, int32_t *uargv_length, error_t **error)
{
    UChar *uargv;
    UConverter *ucnv;
    UErrorCode status;
    int32_t cargv_length;
    int32_t _uargv_length;
    int32_t allocated;

    status = U_ZERO_ERROR;
    ucnv = ucnv_open(env_get_stdin_encoding(), &status);
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
