#include "common.h"
#include "parsenum.h"

#define parse_signed(type, unsigned_type, value_type_min, value_type_max) \
    ParseNumError parse_## type(const char *nptr, char **endptr, int base, type *min, type *max, type *ret) { \
        char c; \
        int negative; \
        const char *s; \
        int any, cutlim; \
        unsigned_type cutoff, acc; \
        ParseNumError err; \
 \
        s = nptr; \
        acc = any = 0; \
        *endptr = NULL; \
        err = PARSE_NUM_NO_ERR; \
        if ('-' == *s) { \
            ++s; \
            negative = TRUE; \
        } else { \
            negative = FALSE; \
            if ('+' == *s) { \
                ++s; \
            } \
        } \
        if ((0 == base || 2 == base) && '0' == *s && ('b' == s[1] || 'B' == s[1])) { \
            s += 2; \
            base = 2; \
        } \
        if ((0 == base || 16 == base) && '0' == *s && ('x' == s[1] || 'X' == s[1])) { \
            s += 2; \
            base = 16; \
        } \
        if (0 == base) { \
            base = '0' == *s ? 8 : 10; \
        } \
        if (base < 2 || base > 36) { \
            return PARSE_NUM_ERR_INVALID_BASE; \
        } \
        cutoff = negative ? (unsigned_type) - (value_type_min + value_type_max) + value_type_max : value_type_max; \
        cutlim = cutoff % base; \
        cutoff /= base; \
        for (/*  NOP */; '\0' != *s; s++) { \
            if (*s >= '0' && *s <= '9') { \
                c = *s - '0'; \
            } else if (base > 10 && *s >= 'A' && *s <= 'Z') { \
                c = *s - 'A' - 10; \
            } else if (base > 10 && *s >= 'a' && *s <= 'z') { \
                c = *s - 'a' - 10; \
            } else { \
                err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                if (NULL != endptr) { \
                    *endptr = (char *) s; \
                } \
                break; \
            } \
            if (c >= base) { \
                err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                if (NULL != endptr) { \
                    *endptr = (char *) s; \
                } \
                break; \
            } \
            if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) { \
                any = -1; \
            } else { \
                any = 1; \
                acc *= base; \
                acc += c; \
            } \
        } \
        if (any < 0) { \
            if (negative) { \
                *ret = value_type_min; \
                return PARSE_NUM_ERR_TOO_SMALL; \
            } else { \
                *ret = value_type_max; \
                return PARSE_NUM_ERR_TOO_LARGE; \
            } \
        } else if (!any && PARSE_NUM_NO_ERR == err) { \
            err = PARSE_NUM_ERR_NO_DIGIT_FOUND; \
        } else if (negative) { \
            *ret = -acc; \
        } else { \
            *ret = acc; \
        } \
        if (PARSE_NUM_NO_ERR == err) { \
            if (NULL != min && *ret < *min) { \
                err = PARSE_NUM_ERR_LESS_THAN_MIN; \
            } \
            if (NULL != max && *ret > *max) { \
                err = PARSE_NUM_ERR_GREATER_THAN_MAX; \
            } \
        } \
 \
        return err; \
    }

parse_signed(int8_t, uint8_t, INT8_MIN, INT8_MAX);
parse_signed(int16_t, uint16_t, INT16_MIN, INT16_MAX);
parse_signed(int32_t, uint32_t, INT32_MIN, INT32_MAX);
parse_signed(int64_t, uint64_t, INT64_MIN, INT64_MAX);

#undef parse_signed

#define parse_unsigned(type, value_type_max) \
    ParseNumError parse_## type(const char *nptr, char **endptr, int base, type *min, type *max, type *ret) { \
        char c; \
        int negative; \
        const char *s; \
        int any, cutlim; \
        type cutoff, acc; \
        ParseNumError err; \
 \
        s = nptr; \
        acc = any = 0; \
        *endptr = NULL; \
        err = PARSE_NUM_NO_ERR; \
        if ('-' == *s) { \
            ++s; \
            negative = TRUE; \
        } else { \
            negative = FALSE; \
            if ('+' == *s) { \
                ++s; \
            } \
        } \
        if ((0 == base || 2 == base) && '0' == *s && ('b' == s[1] || 'B' == s[1])) { \
            s += 2; \
            base = 2; \
        } \
        if ((0 == base || 16 == base) && '0' == *s && ('x' == s[1] || 'X' == s[1])) { \
            s += 2; \
            base = 16; \
        } \
        if (0 == base) { \
            base = '0' == *s ? 8 : 10; \
        } \
        if (base < 2 || base > 36) { \
            return PARSE_NUM_ERR_INVALID_BASE; \
        } \
        cutoff = value_type_max / base; \
        cutlim = value_type_max % base; \
        for (/*  NOP */; '\0' != *s; s++) { \
            if (*s >= '0' && *s <= '9') { \
                c = *s - '0'; \
            } else if (base > 10 && *s >= 'A' && *s <= 'Z') { \
                c = *s - 'A' - 10; \
            } else if (base > 10 && *s >= 'a' && *s <= 'z') { \
                c = *s - 'a' - 10; \
            } else { \
                err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                if (NULL != endptr) { \
                    *endptr = (char *) s; \
                } \
                break; \
            } \
            if (c >= base) { \
                err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                if (NULL != endptr) { \
                    *endptr = (char *) s; \
                } \
                break; \
            } \
            if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) { \
                any = -1; \
            } else { \
                any = 1; \
                acc *= base; \
                acc += c; \
            } \
        } \
        if (any < 0) { \
            *ret = value_type_max; \
            return PARSE_NUM_ERR_TOO_LARGE; \
        } else if (!any && PARSE_NUM_NO_ERR == err) { \
            err = PARSE_NUM_ERR_NO_DIGIT_FOUND; \
        } else if (negative) { \
            *ret = -acc; \
        } else { \
            *ret = acc; \
        } \
        if (PARSE_NUM_NO_ERR == err) { \
            if (NULL != min && *ret < *min) { \
                err = PARSE_NUM_ERR_LESS_THAN_MIN; \
            } \
            if (NULL != max && *ret > *max) { \
                err = PARSE_NUM_ERR_GREATER_THAN_MAX; \
            } \
        } \
 \
        return err; \
    }

parse_unsigned(uint8_t, UINT8_MAX);
parse_unsigned(uint16_t, UINT16_MAX);
parse_unsigned(uint32_t, UINT32_MAX);
parse_unsigned(uint64_t, UINT64_MAX);

#undef parse_unsigned
