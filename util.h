#ifndef UTIL_H

# define UTIL_H

extern UFILE *ustdout;
extern UFILE *ustderr;

extern int verbosity;
extern int exit_failure_value;

# define GETOPT_SPECIFIC (CHAR_MAX + 1)
# define GETOPT_COMMON   0xA0

# define GETOPT_COMMON_OPTIONS                      \
    {"input",  required_argument, NULL, INPUT_OPT}, \
    {"reader", required_argument, NULL, READER_OPT}

enum {
    INPUT_OPT = GETOPT_COMMON,
    READER_OPT
};

UChar *local_to_uchar(const char *, int32_t *, error_t **);
UChar32 *local_to_uchar32(const char *, int32_t *, error_t **);
void print_error(error_t *error);
void report(int type, const char *format, ...);
UBool stdout_is_tty(void);
int32_t u_ltrim(UChar *, int32_t, UChar *, int32_t);
int32_t u_rtrim(UChar *, int32_t, UChar *, int32_t);
int32_t u_trim(UChar *, int32_t, UChar *, int32_t);
//void ustdio_init(void);
# ifndef _MSC_VER
INITIALIZER_DECL(ustdio_init);
# endif /* !_MSC_VER */
UBool util_opt_parse(int, const char *, reader_t *);

#endif /* UTIL_H */
