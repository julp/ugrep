#ifndef UTIL_H

# define UTIL_H

extern UFILE *ustdout;
extern UFILE *ustderr;

extern int verbosity;
extern int exit_failure_value;

UChar *local_to_uchar(const char *, int32_t *, error_t **);
UChar32 *local_to_uchar32(const char *, int32_t *, error_t **);
void print_error(error_t *);
void report(int, const char *, ...);
UBool stdin_is_tty(void);
UBool stdout_is_tty(void);
int32_t u_ltrim(UChar *, int32_t, UChar *, int32_t);
int32_t u_rtrim(UChar *, int32_t, UChar *, int32_t);
int32_t u_trim(UChar *, int32_t, UChar *, int32_t);
void util_apply(void);
const char *util_get_inputs_encoding(void);
const char *util_get_stdin_encoding(void);
void util_set_inputs_encoding(const char *);
void util_set_outputs_encoding(const char *);
void util_set_stdin_encoding(const char *);
void util_set_system_encoding(const char *);
//void ustdio_init(void);
# ifndef _MSC_VER
INITIALIZER_DECL(ustdio_init);
# endif /* !_MSC_VER */

#endif /* UTIL_H */
