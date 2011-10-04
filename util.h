#ifndef UTIL_H

# define UTIL_H

extern UFILE *ustdout;
extern UFILE *ustderr;

extern int verbosity;
extern int exit_failure_value;

# define GETOPT_SPECIFIC (CHAR_MAX + 1)
# define GETOPT_COMMON   0xA0

# define GETOPT_COMMON_OPTIONS                       \
    {"input",  required_argument, NULL, INPUT_OPT},  \
    {"stdin",  required_argument, NULL, STDIN_OPT},  \
    {"output", required_argument, NULL, OUTPUT_OPT}, \
    {"system", required_argument, NULL, SYSTEM_OPT}, \
    {"reader", required_argument, NULL, READER_OPT}

enum {
    INPUT_OPT = GETOPT_COMMON,
    STDIN_OPT,
    OUTPUT_OPT,
    SYSTEM_OPT,
    READER_OPT
};

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
# ifndef _MSC_VER
INITIALIZER_DECL(util_init);
# endif /* !_MSC_VER */
UBool util_opt_parse(int, const char *, reader_t *);

#endif /* UTIL_H */
