#ifndef UTIL_H

# define UTIL_H

extern UFILE *ustdout;
extern UFILE *ustderr;

extern int verbosity;
extern int exit_failure_value;

UBool stdout_is_tty(void);
void print_error(error_t *error);
void report(int type, const char *format, ...);
//void ustdio_init(void);
// # ifdef BINARY
INITIALIZER_DECL(ustdio_init);
// # endif

#endif /* UTIL_H */
