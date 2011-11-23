#include "common.h"
#include "struct/intervals.c"

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
    exit_failure_value = EXIT_FAILURE;
    env_apply();
    error = NULL;
    l = interval_list_new();
    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        interval_list_clean(l);
        if ('!' == tests[i][0]) {
            if (!parseIntervals(&error, tests[i] + 1, l, 0)) {
                print_error(error);
            }
            interval_list_complement(l, 200, 1000);
        } else {
            if (!parseIntervals(&error, tests[i], l, 0)) {
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
