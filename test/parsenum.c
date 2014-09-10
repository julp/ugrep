#include "common.h"
#include <inttypes.h>
#include "parsenum.h"

#define UT(type, string, expected_value, expected_return, base, _min, _max, stopchar) \
    do { \
        int x; \
        char *endptr; \
        type min, max, *pmin, *pmax, v = 0; \
        \
        pmin = pmax = NULL; \
        if (0 != _min) { \
            min = _min; \
            pmin = &min; \
        } \
        if (0 != _max) { \
            max = _max; \
            pmax = &max; \
        } \
        x = parse_ ## type(string, &endptr, base, pmin, pmax, &v); \
        ret &= x == expected_return; \
        if (expected_return != x) { \
           printf("parse_%s(%s): %d returned (%d expected)\n", #type, string, x, expected_return); \
        } \
        ret &= v == expected_value; \
        if (v != expected_value) { \
            /* TODO: PRIi8 vs PRIu8 modifier */ \
            printf("parse_%s(%s): %d parsed (%d expected)\n", #type, string, v, expected_value); \
        } \
        ret &= *endptr == stopchar; \
        if (*endptr != stopchar) { \
            printf("parse_%s(%s): stopped on 0x%02X (0x%02X expected)\n", #type, string, *endptr, stopchar); \
        } \
    } while (0);

int main(void)
{
    int ret;

    ret = TRUE;
    env_init(EXIT_FAILURE);
    env_apply();

    UT(int8_t, "129", INT8_MAX, PARSE_NUM_ERR_TOO_LARGE, 0, 0, 0, '\0');
    UT(int8_t, "", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 0, 0, 0, '\0');
    UT(int8_t, "j10", 0, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, 'j');
    UT(int8_t, "1j0", 1, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, 'j');
    UT(int8_t, "10j", 10, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, 'j');
    UT(int8_t, "5", 5, PARSE_NUM_NO_ERR, 0, 0, 0, '\0');
    UT(int8_t, "2", 2, PARSE_NUM_ERR_LESS_THAN_MIN, 0, 5, 0, '\0');
    UT(int8_t, "0b1001", 9, PARSE_NUM_NO_ERR, 0, 0, 0, '\0');
    UT(int8_t, "0b1001", 9, PARSE_NUM_NO_ERR, 2, 0, 0, '\0');
    UT(int8_t, "0b1201", 1, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, '2');
    UT(int8_t, "0b", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 0, 0, 0, '\0');
    UT(int8_t, "0b", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 2, 0, 0, '\0');
    UT(int8_t, "0b", 0, PARSE_NUM_ERR_NON_DIGIT_FOUND, 10, 0, 0, 'b');
    UT(int8_t, "0x", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 0, 0, 0, '\0');
    UT(int8_t, "0x", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 16, 0, 0, '\0');
    UT(int8_t, "0x", 0, PARSE_NUM_ERR_NON_DIGIT_FOUND, 10, 0, 0, 'x');

    UT(uint8_t, "256", UINT8_MAX, PARSE_NUM_ERR_TOO_LARGE, 0, 0, 0, '\0');
    UT(uint8_t, "-1", UINT8_MAX, PARSE_NUM_NO_ERR, 0, 0, 0, '\0');
    UT(uint8_t, "", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 0, 0, 0, '\0');
    UT(uint8_t, "j10", 0, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, 'j');
    UT(uint8_t, "1j0", 1, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, 'j');
    UT(uint8_t, "10j", 10, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, 'j');
    UT(uint8_t, "5", 5, PARSE_NUM_NO_ERR, 0, 0, 0, '\0');
    UT(uint8_t, "2", 2, PARSE_NUM_ERR_LESS_THAN_MIN, 0, 5, 0, '\0');
    UT(uint8_t, "0b1001", 9, PARSE_NUM_NO_ERR, 0, 0, 0, '\0');
    UT(uint8_t, "0b1001", 9, PARSE_NUM_NO_ERR, 2, 0, 0, '\0');
    UT(uint8_t, "0b1201", 1, PARSE_NUM_ERR_NON_DIGIT_FOUND, 0, 0, 0, '2');
    UT(uint8_t, "0b", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 0, 0, 0, '\0');
    UT(uint8_t, "0b", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 2, 0, 0, '\0');
    UT(uint8_t, "0b", 0, PARSE_NUM_ERR_NON_DIGIT_FOUND, 10, 0, 0, 'b');
    UT(uint8_t, "0x", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 0, 0, 0, '\0');
    UT(uint8_t, "0x", 0, PARSE_NUM_ERR_NO_DIGIT_FOUND, 16, 0, 0, '\0');
    UT(uint8_t, "0x", 0, PARSE_NUM_ERR_NON_DIGIT_FOUND, 10, 0, 0, 'x');

    return (TRUE == ret ? EXIT_SUCCESS : EXIT_FAILURE);
}
