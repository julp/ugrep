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
        case READER_OPT:
            if (!reader_set_imp_by_name(reader, optarg)) {
                fprintf(stderr, "Unknown or unavailable reader\n");
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
