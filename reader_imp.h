#ifndef READER_H

# define READER_H

typedef struct {
    UBool internal;
    const char *name;
    void *(*open)(error_t **, const char *, int);
    void (*close)(void *);
    UBool (*eof)(void *);
    UBool (*seekable)(void *);
    UBool (*readline)(error_t **error, void *, UString *);
    size_t (*readbytes)(void *, char *, size_t);                         /* Caller must append trailing \0 */
    int32_t (*readuchars)(error_t **error, void *, UChar *, size_t);     /* Caller must append trailing \0 */
    int32_t (*readuchars32)(error_t **error, void *, UChar32 *, size_t); /* Caller must append trailing \0 */
    UBool (*has_encoding)(void *);
    const char *(*get_encoding)(error_t **error, void *);
    UBool (*set_encoding)(error_t **, void *, const char *);
    void (*rewind)(void *, int32_t); /* Caller must provide BOM length (as 2nd argument) */
} reader_imp_t;

/*static inline int min(int a, int b)
{
    return (a > b) ? a : b;
}*/

# define STRING_REWIND(start, ptr, signature_length, pendingCU) \
    do {                                                        \
        ptr = start + signature_length;                         \
        pendingCU = 0;                                          \
    } while (0);

# define STRING_READBYTES(ptr, end, buffer, max_len) \
    do {                                             \
        size_t n;                                    \
                                                     \
        if (end - ptr > max_len) {                   \
            n = max_len;                             \
        } else {                                     \
            n = end - ptr;                           \
        }                                            \
        memcpy(buffer, ptr, n);                      \
        ptr += n;                                    \
                                                     \
        return n;                                    \
    } while (0);

# define STRING_READUCHARS(error, ucnv, ptr, end, buffer, max_len, pendingCU)                                      \
    do {                                                                                                           \
        int count;                                                                                                 \
        UErrorCode status;                                                                                         \
        UChar *dest;                                                                                               \
        const UChar *uend;                                                                                         \
                                                                                                                   \
        require_else_return_val(max_len >= 2, -1);                                                                 \
                                                                                                                   \
        status = U_ZERO_ERROR;                                                                                     \
        dest = buffer;                                                                                             \
        uend = buffer + max_len;                                                                                   \
        if (0 != pendingCU) {                                                                                      \
            *dest++ = pendingCU;                                                                                   \
            pendingCU = 0;                                                                                         \
        }                                                                                                          \
        ucnv_toUnicode(ucnv, &dest, uend, (const char **) &ptr, end, NULL, ptr >= end && 0 == pendingCU, &status); \
        if (U_FAILURE(status) && U_BUFFER_OVERFLOW_ERROR != status) {                                              \
            icu_error_set(error, FATAL, status, "ucnv_toUnicode");                                                 \
            return -1;                                                                                             \
        }                                                                                                          \
        count = dest - buffer;                                                                                     \
        if (count == max_len && !U16_IS_SINGLE(buffer[count - 1]) && U16_IS_LEAD(buffer[count - 1])) {             \
            pendingCU = buffer[--count];                                                                           \
        }                                                                                                          \
                                                                                                                   \
        return count;                                                                                              \
    } while (0);

# define STRING_READUCHARS32(error, ucnv, ptr, end, buffer, max_len)         \
    do {                                                                     \
        UChar32 c;                                                           \
        int32_t i;                                                           \
        UErrorCode status;                                                   \
                                                                             \
        status = U_ZERO_ERROR;                                               \
        for (i = 0; i < max_len && ptr < end; i++) {                         \
            c = ucnv_getNextUChar(ucnv, (const char **) &ptr, end, &status); \
            if (U_FAILURE(status)) {                                         \
                icu_error_set(error, FATAL, status, "ucnv_getNextUChar");    \
                return -1;                                                   \
            }                                                                \
            buffer[i] = c;                                                   \
        }                                                                    \
                                                                             \
        return i;                                                            \
    } while (0);

# define STRING_READLINE(error, ucnv, ptr, end, ustr)                         \
    do {                                                                      \
        UChar32 c;                                                            \
        UErrorCode status;                                                    \
                                                                              \
        status = U_ZERO_ERROR;                                                \
        do {                                                                  \
            c = ucnv_getNextUChar(ucnv, (const char **) &ptr, end, &status);  \
            if (U_FAILURE(status)) {                                          \
                if (U_INDEX_OUTOFBOUNDS_ERROR == status) { /* c == U_EOF */   \
                    break;                                                    \
                } else {                                                      \
                    icu_error_set(error, FATAL, status, "ucnv_getNextUChar"); \
                    return FALSE;                                             \
                }                                                             \
            }                                                                 \
            ustring_append_char(ustr, c);                                     \
        } while (U_LF != c);                                                  \
                                                                              \
        return TRUE;                                                          \
    } while (0);

# define STRING_HAS_ENCODING(ucnv) \
    do {                           \
        return NULL != ucnv;       \
    } while (0);

# define STRING_GET_ENCODING(error, ucnv)                        \
    do {                                                         \
        UErrorCode status;                                       \
        const char *encoding;                                    \
                                                                 \
        status = U_ZERO_ERROR;                                   \
        encoding = ucnv_getName(ucnv, &status);                  \
        if (U_SUCCESS(status)) {                                 \
            return encoding;                                     \
        } else {                                                 \
            icu_error_set(error, FATAL, status, "ucnv_getName"); \
            return NULL;                                         \
        }                                                        \
    } while (0);

# define STRING_SET_ENCODING(error, ucnv, encoding) \
    do {                                            \
        UErrorCode status;                                    \
                                                              \
        status = U_ZERO_ERROR;                                \
        ucnv = ucnv_open(encoding, &status);                  \
        if (U_FAILURE(status)) {                              \
            icu_error_set(error, FATAL, status, "ucnv_open"); \
        }                                                     \
                                                              \
        return U_SUCCESS(status);                             \
    } while (0);

# define STRING_EOF(ptr, end, pendingCU)     \
    do {                                     \
        return ptr >= end && 0 == pendingCU; \
    } while (0);

# define STRING_SEEKABLE() \
    do {                   \
        return TRUE;       \
    } while (0);

#endif /* READER_H */
