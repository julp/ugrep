#include <unistd.h>
#ifdef _MSC_VER
# define STRICT
# include <windows.h>
# include <direct.h>
# include <Winreg.h>
# include <psapi.h>
# pragma comment(lib,"Psapi.lib")
char __progname[_MAX_PATH] = "<unknown>";
#endif /* _MSC_VER */

#include "common.h"

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
    return (1 == isatty(STDOUT_FILENO));
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

INITIALIZER_P(ustdio_init)
{
#ifdef _MSC_VER
    GetModuleBaseNameA(GetCurrentProcess(), NULL, __progname,  sizeof(__progname)/sizeof(char));
    if (stdout_is_tty()) {
        HKEY hkey;
        char cp[30] = "";
        DWORD cp_len;

        cp_len = sizeof(cp) / sizeof(char);
        if (ERROR_SUCCESS == RegOpenKeyExA(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Control\\Nls\\CodePage"), 0, KEY_QUERY_VALUE, &hkey)) {
            if (ERROR_SUCCESS == RegQueryValueExA(hkey, TEXT("OEMCP"), NULL, NULL, (LPBYTE) &cp, &cp_len)) {
                cp[cp_len] = '\0';
            }
            RegCloseKey(hkey);
        }
        ustdout = u_finit(stdout, NULL, cp);
        ustderr = u_finit(stderr, NULL, cp);
    } else
#endif /* _MSC_VER */
    {
        ustdout = u_finit(stdout, NULL, NULL);
        ustderr = u_finit(stderr, NULL, NULL);
    }

    debug("system locale = " YELLOW("%s"), u_fgetlocale(ustdout));
    debug("system codepage = " YELLOW("%s"), u_fgetcodepage(ustdout));
}
