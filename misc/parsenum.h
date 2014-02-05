
typedef enum {
    PARSE_NUM_NO_ERR = 0,
    PARSE_NUM_ERR_OUT_OF_RANGE,     // for int32_t, value > INT32_MAX || value < INT32_MIN
    PARSE_NUM_ERR_NO_DIGIT_FOUND,   // s = endptr <=> empty string <=> nothing consumed
    PARSE_NUM_ERR_NON_DIGIT_FOUND,  // non digit found (eg "j10")
    PARSE_NUM_ERR_INVALID_BASE,     // base is invalid (base < 2 || base > 36)
//     PARSE_NUM_ERR_INVALID_DIGIT,    // digit is invalid for given base
    PARSE_NUM_ERR_LESS_THAN_MIN,    // value is less than user specified value
    PARSE_NUM_ERR_GREATER_THAN_MAX, // value is greater than user specified value
    __PARSE_NUM_ERR_COUNT           // private, number of errors
} ParseNumError;

# define parse_signed(type, unsigned_type, value_type_min, value_type_max) \
    ParseNumError parse_## type(const char *, char **, int, type *, type *, type *)

parse_signed(int8_t, uint8_t, INT8_MIN, INT8_MAX);
parse_signed(int16_t, uint16_t, INT16_MIN, INT16_MAX);
parse_signed(int32_t, uint32_t, INT32_MIN, INT32_MAX);
parse_signed(int64_t, uint64_t, INT64_MIN, INT64_MAX);

#undef parse_signed

# define parse_unsigned(type, value_type_max) \
    ParseNumError parse_## type(const char *, char **, int, type *, type *, type *)

parse_unsigned(uint8_t, UINT8_MAX);
parse_unsigned(uint16_t, UINT16_MAX);
parse_unsigned(uint32_t, UINT32_MAX);
parse_unsigned(uint64_t, UINT64_MAX);

#undef parse_unsigned
