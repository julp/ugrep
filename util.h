#ifndef UTIL_H

# define UTIL_H

extern UFILE *ustdout;
extern UFILE *ustderr;

extern int verbosity;
extern int exit_failure_value;

void print_error(error_t *error);
void report(int type, const char *format, ...);
void ustdio_init(void);

#endif /* UTIL_H */
