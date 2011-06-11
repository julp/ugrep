#ifndef UTIL_H

# define UTIL_H

extern UFILE *ustdout;
extern UFILE *ustderr;

extern int verbosity;
extern int exit_failure_value;

UChar *convert_argv_from_local(const char *, int32_t *, error_t **);
void print_error(error_t *error);
void report(int type, const char *format, ...);
UBool stdout_is_tty(void);
int32_t u_ltrim(UChar *, int32_t, UChar *, int32_t);
int32_t u_rtrim(UChar *, int32_t, UChar *, int32_t);
int32_t u_trim(UChar *, int32_t, UChar *, int32_t);
//void ustdio_init(void);
INITIALIZER_DECL(ustdio_init);

#endif /* UTIL_H */
