#ifndef UTIL_H

# define UTIL_H

extern int verbosity;
extern int exit_failure_value;

# define GETOPT_SPECIFIC (CHAR_MAX + 1)
# define GETOPT_COMMON   0xA0

# define GETOPT_COMMON_OPTIONS                       \
    {"input",  required_argument, NULL, INPUT_OPT},  \
    {"stdin",  required_argument, NULL, STDIN_OPT},  \
    {"output", required_argument, NULL, OUTPUT_OPT}, \
    {"system", required_argument, NULL, SYSTEM_OPT}, \
    {"nfnone", no_argument,       NULL, NFNONE_OPT}, \
    {"nfd",    no_argument,       NULL, NFD_OPT},    \
    {"nfc",    no_argument,       NULL, NFC_OPT},    \
    {"reader", required_argument, NULL, READER_OPT}

enum {
    INPUT_OPT = GETOPT_COMMON,
    STDIN_OPT,
    OUTPUT_OPT,
    SYSTEM_OPT,
    NFNONE_OPT,
    NFD_OPT,
    NFC_OPT,
    READER_OPT
};

UChar *local_to_uchar(const char *, int32_t *, error_t **);
UChar32 *local_to_uchar32(const char *, int32_t *, error_t **);
void print_error(error_t *);
void report(int, const char *, ...);
UBool stdin_is_tty(void);
UBool stdout_is_tty(void);

UBool util_opt_parse(int, const char *, reader_t *);

#endif /* UTIL_H */
