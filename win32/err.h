#ifndef ERR_H

# define ERR_H

# include <stdarg.h>

void warnx(const char *fmt, ...);
void vwarnx(const char *fmt, va_list ap);

#endif /* !ERR_H */
