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
        err = PARSE_NUM_NO_ERR; \
        if ('-' == (c = *s++)) { \
            negative = TRUE; \
            c = *s++; \
        } else { \
            negative = FALSE; \
            if ('+' == *s) { \
                c = *s++; \
            } \
        } \
        if ((0 == base || 2 == base) && '0' == c && ('b' == *s || 'B' == *s) && ('0' == s[1] || '1' == s[1])) { \
            c = s[1]; \
            s += 2; \
            base = 2; \
        } \
        if ((0 == base || 16 == base) && '0' == c && ('x' == *s || 'X' == *s) && ((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F') || (s[1] >= 'a' && s[1] <= 'f'))) { \
            c = s[1]; \
            s += 2; \
            base = 16; \
        } \
        if (0 == base) { \
            base = '0' == c ? 8 : 10; \
        } \
        if (base < 2 || base > 36) { \
            return PARSE_NUM_ERR_INVALID_BASE; \
        } \
        cutoff = negative ? (unsigned_type) - (value_type_min + value_type_max) + value_type_max : value_type_max; \
        cutlim = cutoff % base; \
        cutoff /= base; \
        do { \
            if (c >= '0' && c < ('0' + base)/*c <= '9'*/) { \
                c -= '0'; \
            } else if (base > 10 && c >= 'A' && c < ('A' + base - 10)/*c <= 'Z'*/) { \
                c -= 'A' - 10; \
            } else if (base > 10 && c >= 'a' && c < ('a' + base - 10)/*c <= 'z'*/) { \
                c -= 'a' - 10; \
            } else { \
                err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                break; \
            } \
            /*if (c >= base) { \
                err = PARSE_NUM_ERR_INVALID_DIGIT; \
                break; \
            }*/ \
            if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) { \
                any = -1; \
            } else { \
                any = 1; \
                acc *= base; \
                acc += c; \
            } \
        } while ('\0' != (c = *s++)); \
        if (NULL != endptr) { \
            *endptr = (char *) (any ? s - 1 : nptr); \
        } \
        if (any < 0) { \
            *ret = negative ? value_type_min : value_type_max; \
            return PARSE_NUM_ERR_OUT_OF_RANGE; \
        } else if (!any) { \
            err = '\0' == *nptr ? PARSE_NUM_ERR_NO_DIGIT_FOUND : PARSE_NUM_ERR_NON_DIGIT_FOUND; \
        } else if (negative) { \
            *ret = -acc; \
        } else { \
            *ret = acc; \
        } \
        if (PARSE_NUM_NO_ERR == err) { \
            if (NULL != min && *ret < *min) { \
                /**endptr = (char *) nptr;*/ \
                err = PARSE_NUM_ERR_LESS_THAN_MIN; \
            } \
            if (NULL != max && *ret > *max) { \
                /**endptr = (char *) nptr;*/ \
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
        err = PARSE_NUM_NO_ERR; \
        if ('-' == (c = *s++)) { \
            negative = TRUE; \
            c = *s++; \
        } else { \
            negative = FALSE; \
            if ('+' == *s) { \
                c = *s++; \
            } \
        } \
        if ((0 == base || 2 == base) && '0' == c && ('b' == *s || 'B' == *s) && ('0' == s[1] || '1' == s[1])) { \
            c = s[1]; \
            s += 2; \
            base = 2; \
        } \
        if ((0 == base || 16 == base) && '0' == c && ('x' == *s || 'X' == *s) && ((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F') || (s[1] >= 'a' && s[1] <= 'f'))) { \
            c = s[1]; \
            s += 2; \
            base = 16; \
        } \
        if (0 == base) { \
            base = '0' == c ? 8 : 10; \
        } \
        if (base < 2 || base > 36) { \
            return PARSE_NUM_ERR_INVALID_BASE; \
        } \
        cutoff = value_type_max / base; \
        cutlim = value_type_max % base; \
        do { \
            if (c >= '0' && c < ('0' + base)/*c <= '9'*/) { \
                c -= '0'; \
            } else if (base > 10 && c >= 'A' && c < ('A' + base - 10)/*c <= 'Z'*/) { \
                c -= 'A' - 10; \
            } else if (base > 10 && c >= 'a' && c < ('a' + base - 10)/*c <= 'z'*/) { \
                c -= 'a' - 10; \
            } else { \
                err = PARSE_NUM_ERR_NON_DIGIT_FOUND; \
                break; \
            } \
            /*if (c >= base) { \
                err = PARSE_NUM_ERR_INVALID_DIGIT; \
                break; \
            }*/ \
            if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) { \
                any = -1; \
            } else { \
                any = 1; \
                acc *= base; \
                acc += c; \
            } \
        } while ('\0' != (c = *s++)); \
        if (NULL != endptr) { \
            *endptr = (char *) (any ? s - 1 : nptr); \
        } \
        if (any < 0) { \
            *ret = value_type_max; \
            return PARSE_NUM_ERR_OUT_OF_RANGE; \
        } else if (!any) { \
            err = '\0' == *nptr ? PARSE_NUM_ERR_NO_DIGIT_FOUND : PARSE_NUM_ERR_NON_DIGIT_FOUND; \
        } else if (negative) { \
            *ret = -acc; \
        } else { \
            *ret = acc; \
        } \
        if (PARSE_NUM_NO_ERR == err) { \
            if (NULL != min && *ret < *min) { \
                /**endptr = (char *) nptr;*/ \
                err = PARSE_NUM_ERR_LESS_THAN_MIN; \
            } \
            if (NULL != max && *ret > *max) { \
                /**endptr = (char *) nptr;*/ \
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

#if 0
ParseNumError parse_int32(const char *nptr, char **endptr, int base, int32_t *min, int32_t *max, int32_t *ret) {
    char c;
    int negative;
    const char *s;
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
    if ((0 == base || 2 == base) && '0' == c && ('b' == *s || 'B' == *s) && ('0' == s[1] || '1' == s[1])) {
        c = s[1];
        s += 2;
        base = 2;
    }
    if ((0 == base || 16 == base) && '0' == c && ('x' == *s || 'X' == *s) && ((s[1] >= '0' && s[1] <= '9') || (s[1] >= 'A' && s[1] <= 'F') || (s[1] >= 'a' && s[1] <= 'f'))) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (0 == base) {
        base = '0' == c ? 8 : 10;
    }
    if (base < 2 || base > 36) {
        return PARSE_NUM_INVALID_OR_UNDETERMINED_BASE;
    }
    cutoff = negative ? (uint32_t) - (INT32_MIN + INT32_MAX) + INT32_MAX : INT32_MAX;
    cutlim = cutoff % base;
    cutoff /= base;
    do {
        if (c >= '0' && c <= '9') {
            c -= '0';
        } else if (c >= 'A' && c <= 'Z') {
            c -= 'A' - 10;
        } else if (c >= 'a' && c <= 'z') {
            c -= 'a' - 10;
        } else {
            break;
        }
        if (c >= base) {
            break;
        }
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc *= base;
            acc += c;
        }
    } while ('\0' != (c = *s++));
    if (NULL != endptr) {
        *endptr = (char *) (any ? s - 1 : nptr);
    }
    if (any < 0) {
        *ret = negative ? INT32_MIN : INT32_MAX;
        return PARSE_NUM_ERR_OUT_OF_RANGE;
    } else if (!any) {
        return PARSE_NUM_ERR_NON_DIGIT_FOUND;
    } else if (negative) {
        *ret = -acc;
    } else {
        *ret = acc;
    }
    if (NULL != min && *ret < *min) {
        *endptr = (char *) nptr;
        return PARSE_NUM_ERR_LESS_THAN_MIN;
    }
    if (NULL != max && *ret > *max) {
        *endptr = (char *) nptr;
        return PARSE_NUM_ERR_GREATER_THAN_MAX;
    }

    return PARSE_NUM_NO_ERR;
}
#endif
