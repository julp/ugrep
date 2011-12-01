#include <getopt.h>

#include "common.h"


static char optstr[] = "";

static struct option long_options[] =
{
    GETOPT_COMMON_OPTIONS
};


static void usage(void)
{
    fprintf(
        stderr,
        "usage: %s [file ...]\n",
        __progname
    );
    exit(2);
}

static int procfile(reader_t *reader, const char *filename)
{
    int32_t read;
    error_t *error;
    UChar buffer[2];

    error = NULL;

    if (reader_open(reader, &error, filename)) {
        while ((read = reader_readuchars(reader, &error, buffer, (int32_t) ARRAY_SIZE(buffer))) > 0) {
            u_fprintf(ustdout, ">%.*S< (%d)\n", read, buffer, read);
        }
        reader_close(reader);
    }

    if (NULL != error) {
        print_error(error);
        return 1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int c, ret;
    reader_t reader;

    env_init(EXIT_FAILURE);
    reader_init(&reader, DEFAULT_READER_NAME);

    while (-1 != (c = getopt_long(argc, argv, optstr, long_options, NULL))) {
        if (!util_opt_parse(c, optarg, &reader)) {
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    env_apply();

    ret = 0;
    if (0 == argc) {
        ret |= procfile(&reader, "-");
    } else {
        for ( ; argc--; ++argv) {
            ret |= procfile(&reader, *argv);
        }
    }

    return (0 == ret ? EXIT_SUCCESS : EXIT_FAILURE);
}
