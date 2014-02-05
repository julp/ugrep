#include <getopt.h>

#include "common.h"

#include <unicode/ubrk.h>

enum {
    //BYTES,
    CODEPOINTS = 1<<0,
    GRAPHEMES = 1<<1,
    WORDS = 1<<2,
    LINES = 1<<3,
};

enum {
    UWC_EXIT_SUCCESS = 0,
    UWC_EXIT_FAILURE,
    UWC_EXIT_USAGE
};

/* ========== getopt stuff ========== */

static char optstr[] = "Lclmw";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS,
    {"xxx", no_argument, NULL, 'x'},
    {NULL,  no_argument, NULL, 0}
};

static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s TODO\n",
        __progname
    );
    exit(UWC_EXIT_USAGE);
}

/* ========== main ========== */

int main(int argc, char **argv)
{
    int mode;
    int c, ret;
    error_t *error;
    reader_t *reader;

    ret = 0;
    mode = 0;
    error = NULL;
    env_init(UWC_EXIT_FAILURE);
    reader = reader_new(DEFAULT_READER_NAME);

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        switch (c) {
            case 'L':
                // longest line
                break;
            case 'c':
                // unsupported, no sense
                fprintf(stderr, "Working with bytes makes no sense: %s works in UTF-16, after a possible charset conversion and normalization\n", "uwc");
                break;
            case 'l':
                mode |= LINES;
                break;
            case 'm':
                mode |= GRAPHEMES;
                break;
            case 'w':
                mode |= WORDS;
                break;
            default:
                if (!util_opt_parse(c, optarg, reader)) {
                    usage();
                }
                break;
        }
    }
    argc -= optind;
    argv += optind;

    env_apply();

    // process
#if 1
# include <parsenum.h>
# include <inttypes.h>
    {
        char *endptr;
        int8_t v, min = 5;
        v = 0;
        printf("%d/%" PRIi8 " = parse_int8_t('129') %c\n", parse_int8_t("129", &endptr, 0, NULL, NULL, &v), v, '\0' == *endptr ? '-' : *endptr);
        v = 0;
        printf("%d/%" PRIi8 " = parse_int8_t('') %c\n", parse_int8_t("", &endptr, 0, NULL, NULL, &v), v, '\0' == *endptr ? '-' : *endptr);
        v = 0;
        printf("%d/%" PRIi8 " = parse_int8_t('j10') %c\n", parse_int8_t("j10", &endptr, 0, NULL, NULL, &v), v, '\0' == *endptr ? '-' : *endptr);
        v = 0;
        printf("%d/%" PRIi8 " = parse_int8_t('5') %c\n", parse_int8_t("5", &endptr, 0, NULL, NULL, &v), v, '\0' == *endptr ? '-' : *endptr);
        v = 0;
        printf("%d/%" PRIi8 " = parse_int8_t('2', min = 5) %c\n", parse_int8_t("2", &endptr, 0, &min, NULL, &v), v, '\0' == *endptr ? '-' : *endptr);
        v = 0;
        printf("%d/%" PRIi8 " = parse_int8_t('0b1001') %c\n", parse_int8_t("0b1001", &endptr, 0, NULL, NULL, &v), v, '\0' == *endptr ? '-' : *endptr);
        v = 0;
        printf("%d/%" PRIi8 " = parse_int8_t('0b1201') %c\n", parse_int8_t("0b1201", &endptr, 0, NULL, NULL, &v), v, '\0' == *endptr ? '-' : *endptr);
    }
    {
        printf("\n");
    }
    {
        uint8_t v, min = 5;
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('256')\n", parse_uint8_t("256", NULL, 0, NULL, NULL, &v), v);
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('-1')\n", parse_uint8_t("-1", NULL, 0, NULL, NULL, &v), v);
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('')\n", parse_uint8_t("", NULL, 0, NULL, NULL, &v), v);
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('j10')\n", parse_uint8_t("j10", NULL, 0, NULL, NULL, &v), v);
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('5')\n", parse_uint8_t("5", NULL, 0, NULL, NULL, &v), v);
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('2', min = 5)\n", parse_uint8_t("2", NULL, 0, &min, NULL, &v), v);
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('0b1001')\n", parse_uint8_t("0b1001", NULL, 0, NULL, NULL, &v), v);
        v = 0;
        printf("%d/%" PRIu8 " = parse_uint8_t('0b1201')\n", parse_uint8_t("0b1201", NULL, 0, NULL, NULL, &v), v);
    }
#endif

    return (0 == ret ? UWC_EXIT_SUCCESS : UWC_EXIT_FAILURE);
}
