#include "common.h"
#include "struct/intervals.c"

enum {
    FIELD_NO_ERR = 0,
    FIELD_ERR_NUMBER_EXPECTED, // s == *endptr
    FIELD_ERR_OUT_OF_RANGE,    // number not in [min;max] ([1;INT_MAX] here)
    FIELD_ERR_NON_DIGIT_FOUND, // *endptr not in ('\0', ',')
    FIELD_ERR_INVALID_RANGE,   // lower_limit > upper_limit
    FIELD_ERR__COUNT
};

static const char *intervalParsingErrorName(int code)
{
    switch (code) {
        case FIELD_NO_ERR:
            return "no error";
        case FIELD_ERR_NUMBER_EXPECTED:
            return "number expected";
        case FIELD_ERR_OUT_OF_RANGE:
            return "number is out of the range [1;INT32_MAX[";
        case FIELD_ERR_NON_DIGIT_FOUND:
            return "non digit character found";
        case FIELD_ERR_INVALID_RANGE:
            return "invalid range: upper limit should be greater or equal than lower limit";
        default:
            return "bogus error code";
    }
}

#ifndef HAVE_STRCHRNUL
static char *strchrnul(const char *s, int c)
{
    while (('\0' != *s) && (*s != c)) {
        s++;
    }

    return (char *) s;
}
#endif /* HAVE_STRCHRNUL */

static int parseIntervalBoundary(const char *nptr, char **endptr, int32_t min, int32_t max, int32_t *ret)
{
    char c;
    const char *s;
    UBool negative;
    int any, cutlim;
    uint32_t cutoff, acc;

    s = nptr;
    acc = any = 0;
    if ('-' == (c = *s++)) {
        negative = TRUE;
        c = *s++;
    } else {
        negative = FALSE;
        if ('+' == *s) {
            c = *s++;
        }
    }
    cutoff = negative ? (uint32_t) - (INT32_MIN + INT32_MAX) + INT32_MAX : INT32_MAX;
    cutlim = cutoff % 10;
    cutoff /= 10;
    do {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else {
            break;
        }
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= 10;
            acc += c;
        }
    } while ('\0' != (c = *s++));
    if (NULL != endptr) {
        *endptr = (char *) (any ? s - 1 : nptr);
    }
    if (any < 0) {
        *ret = negative ? INT32_MIN : INT32_MAX;
        return FIELD_ERR_OUT_OF_RANGE;
    } else if (!any) {
        return FIELD_ERR_NUMBER_EXPECTED;
    } else if (negative) {
        *ret = -acc;
    } else {
        *ret = acc;
    }
    if (*ret < min || *ret > max) {
        return FIELD_ERR_OUT_OF_RANGE;
    }

    return FIELD_NO_ERR;
}

static UBool parseIntervals(error_t **error, const char *s, interval_list_t *intervals)
{
    int ret;
    char *endptr;
    const char *p, *comma;
    int32_t lower_limit, upper_limit;

    p = s;
    while ('\0' != *p) {
        lower_limit = 0;
        upper_limit = INT32_MAX;
        comma = strchrnul(p, ',');
        if ('-' == *p) {
            /* -Y */
            if (0 != (ret = parseIntervalBoundary(p + 1, &endptr, 0, INT32_MAX, &upper_limit)) || ('\0' != *endptr && ',' != *endptr)) {
                error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                return FALSE;
            }
        } else {
            if (NULL == memchr(p, '-', comma - p)) {
                /* X */
                if (0 != (ret = parseIntervalBoundary(p, &endptr, 0, INT32_MAX, &lower_limit)) || ('\0' != *endptr && ',' != *endptr)) {
                    error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                    return FALSE;
                }
                upper_limit = lower_limit;
            } else {
                /* X- or X-Y */
                if (0 != (ret = parseIntervalBoundary(p, &endptr, 0, INT32_MAX, &lower_limit))) {
                    error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                    return FALSE;
                }
                if ('-' == *endptr) {
                    if ('\0' == *(endptr + 1)) {
                        // NOP (lower_limit = 0)
                    } else {
                        if (0 != (ret = parseIntervalBoundary(endptr + 1, &endptr, 0, INT32_MAX, &upper_limit)) || ('\0' != *endptr && ',' != *endptr)) {
                            error_set(error, FATAL, "%s:\n%s\n%*c", intervalParsingErrorName(ret), s, endptr - s + 1, '^');
                            return FALSE;
                        }
                    }
                } else {
                    error_set(error, FATAL, "'-' expected, get '%c' (%d):\n%*c", *endptr, *endptr, endptr - s + 1, '^');
                    return FALSE;
                }
            }
            if (lower_limit > upper_limit) {
                error_set(error, FATAL, "invalid interval: lower limit greater then upper one");
                return FALSE;
            }
        }
//         debug("add [%d;%d[", lower_limit, upper_limit);
        interval_list_add(intervals, INT32_MAX, lower_limit, upper_limit);
        if ('\0' == *comma) {
            break;
        }
        p = comma + 1;
    }

    return TRUE;
}

int ut(interval_list_t *l, interval_t *array) // 0: pass, 1: failed
{
    int j, ret;
    dlist_element_t *el;

    ret = 0;
    for (j = 0, el = l->head; NULL != el; el = el->next, j++) {
        FETCH_DATA(el->data, i, interval_t);

//         debug("[%d;%d[", i->lower_limit, i->upper_limit);
        if (i->lower_limit != array[j].lower_limit || i->upper_limit != array[j].upper_limit) {
            return 1;
        }
    }
    if (array[j].lower_limit != array[j].upper_limit || NULL != el) {
        return 1;
    }

    return ret;
}

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define EOL { -1, -1 }

int main(void)
{
    size_t i;
    int ret, r;
    error_t *error;
    interval_list_t *l;

    const char *tests[] = {
        /* 01 */ "0-100,200-300",
        /* 02 */ "200-300,0-100",
        /* 03 */ "0-100,200-300,400-500,600-700,150-175",
        /* 04 */ "200-300,400-500,600-700,0-100",
        /* 05 */ "200-300,400-500,600-700,800-900",
        /* 06 */ "0-100,200-300,400-500,600-700,50-150",
        /* 07 */ "0-100,200-300,400-500,600-700,50-250",
        /* 08 */ "1,-3",
        /* 09 */ "3,1-",
        /* 10 */ "200-300,400-500,600-700,50-1000",
        /* 11 */ "0-100,200-300,400-500,600-700,50-1000",
        /* 12 */ "200-300,400-500,600-700,800-900,50-850",
        /* 13 */ "!400-500,600-700,800-900",
        /* 14 */ "!200-300,400-500,600-700,800-900",
        /* 15 */ "!400-500,600-700,800-1000",
        /* 16 */ "!200-300,400-500,600-700,800-900,900-1000",
        /* 17 */ "!-10,20-30,40-50,60-70,80-90,400-500,600-700,800-900",
        /* 18 */ "!100-500,600-700,800-900",
        /* 19 */ "!400-500,600-700,800-1200",
        /* 20 */ "!400-500,600-700,800-900,1100-1200,1300-1400",
        /* 21 */ "!-50,100-150,200-300,400-500,600-700,800-900,900-1100",
        /* 22 */ "!100-300,400-500,600-700,800-900,1100-1200,1300-1400",
        /* 23 */ "!0-2147483647" /*TOSTRING(INT32_MAX)*/,
        /* 24 */ "!"
    };
    interval_t results[][16] = {
        /* 01 */ { {0, 100}, {200, 300}, EOL },
        /* 02 */ { {0, 100}, {200, 300}, EOL },
        /* 03 */ { {0, 100}, {150, 175}, {200, 300}, {400, 500}, {600, 700}, EOL },
        /* 04 */ { {0, 100}, {200, 300}, {400, 500}, {600, 700}, EOL },
        /* 05 */ { {200, 300}, {400, 500}, {600, 700}, {800, 900}, EOL },
        /* 06 */ { {0, 150}, {200, 300}, {400, 500}, {600, 700}, EOL },
        /* 07 */ { {0, 300}, {400, 500}, {600, 700}, EOL },
        /* 08 */ { {0, 3}, EOL },
        /* 09 */ { {1, INT32_MAX}, EOL },
        /* 10 */ { {50, 1000}, EOL },
        /* 11 */ { {0, 1000}, EOL },
        /* 12 */ { {50, 900}, EOL },
        /* 13 */ { {200, 400}, {500, 600}, {700, 800}, {900, 1000}, EOL },
        /* 14 */ { {300, 400}, {500, 600}, {700, 800}, {900, 1000}, EOL },
        /* 15 */ { {200, 400}, {500, 600}, {700, 800}, EOL },
        /* 16 */ { {300, 400}, {500, 600}, {700, 800}, EOL },
        /* 17 */ { {200, 400}, {500, 600}, {700, 800}, {900, 1000}, EOL },
        /* 18 */ { {500, 600}, {700, 800}, {900, 1000}, EOL },
        /* 19 */ { {200, 400}, {500, 600}, {700, 800}, EOL },
        /* 20 */ { {200, 400}, {500, 600}, {700, 800}, {900, 1000}, EOL },
        /* 21 */ { {300, 400}, {500, 600}, {700, 800}, EOL },
        /* 22 */ { {300, 400}, {500, 600}, {700, 800}, {900, 1000}, EOL },
        /* 23 */ { EOL },
        /* 24 */ { {200, 1000}, EOL }
    };

    ret = 0;
    env_init();
    env_apply();
    error = NULL;
    l = interval_list_new();
    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        interval_list_clean(l);
        if ('!' == tests[i][0]) {
            if (!parseIntervals(&error, tests[i] + 1, l)) {
                print_error(error);
            }
            interval_list_complement(l, 200, 1000);
        } else {
            if (!parseIntervals(&error, tests[i], l)) {
                print_error(error);
            }
        }
        r = ut(l, results[i]);
        printf("Test %02d: %s\n", i + 1, r ? RED("KO") : GREEN("OK"));
        ret |= r;
    }
    interval_list_destroy(l);

    return (0 == ret ? EXIT_SUCCESS : EXIT_FAILURE);
}
