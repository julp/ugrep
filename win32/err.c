#include <stdio.h>
#include "ugrep.h"
#include "err.h"

void warnx(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vwarnx(fmt, ap);
    va_end(ap);
}

void vwarnx(const char *fmt, va_list ap)
{
    fprintf(stderr, "%s: ", __progname);
    if (NULL != fmt) {
        vfprintf(stderr, fmt, ap);
    }
    fprintf(stderr, "\n");
}
