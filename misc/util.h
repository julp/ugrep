#ifndef UTIL_H

# define UTIL_H

# include <unicode/ubrk.h>
# include <unicode/uregex.h>
# include <unicode/usearch.h>

# define GETOPT_SPECIFIC (CHAR_MAX + 1)
# define GETOPT_COMMON   0xA0

# define GETOPT_COMMON_OPTIONS                       \
    {"input",  required_argument, NULL, INPUT_OPT},  \
    {"stdin",  required_argument, NULL, STDIN_OPT},  \
    {"output", required_argument, NULL, OUTPUT_OPT}, \
    {"system", required_argument, NULL, SYSTEM_OPT}, \
    {"form",   required_argument, NULL, FORM_OPT},   \
    {"unit",   required_argument, NULL, UNIT_OPT},   \
    {"reader", required_argument, NULL, READER_OPT}

# ifdef WITH_FTS
enum {
    DIR_READ,
    DIR_SKIP,
    DIR_RECURSE
};

#  define FTS_COMMON_OPTIONS_STRING "d:D:rRpOS"

#  define FTS_COMMON_OPTIONS                                        \
    {"exclude",     required_argument, NULL, FTS_EXCLUDE_FILE_OPT}, \
    {"include",     required_argument, NULL, FTS_INCLUDE_FILE_OPT}, \
    {"exclude-dir", required_argument, NULL, FTS_EXCLUDE_DIR_OPT},  \
    {"include-dir", required_argument, NULL, FTS_INCLUDE_DIR_OPT},  \
    {"recursive",   no_argument,       NULL, 'r'},                  \
    {"devices",     required_argument, NULL, 'D'},                  \
    {"directories", required_argument, NULL, 'd'}
# else
#  define FTS_COMMON_OPTIONS_STRING ""
# endif /* WITH_FTS */

enum {
    INPUT_OPT = GETOPT_COMMON,
    STDIN_OPT,
    OUTPUT_OPT,
    SYSTEM_OPT,
# ifdef WITH_FTS
    FTS_INCLUDE_DIR_OPT,
    FTS_EXCLUDE_DIR_OPT,
    FTS_INCLUDE_FILE_OPT,
    FTS_EXCLUDE_FILE_OPT,
# endif /* WITH_FTS */
    FORM_OPT,
    UNIT_OPT,
    READER_OPT
};

# ifdef WITH_FTS
int get_dirbehave(void);
UBool skip_file(int);
UBool is_file_matching(char *);
int procdir(reader_t *, char **, void *, int (*procfile)(reader_t *, const char *, void *));
# endif /* WITH_FTS */
UBool stdin_is_tty(void);
UBool stdout_is_tty(void);
void ubrk_unbindText(UBreakIterator *);
void uregex_unbindText(URegularExpression *);
void usearch_unbindText(UStringSearch *);
UBool util_opt_parse(int, const char *, reader_t *);

#endif /* UTIL_H */
