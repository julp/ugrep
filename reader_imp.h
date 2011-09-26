#ifndef READER_H

# define READER_H

typedef struct {
    UBool internal;
    const char *name;
    void *(*open)(error_t **, const char *, int);
    void (*close)(void *);
    UBool (*eof)(void *);
    UBool (*seekable)(void *);
    UBool (*readuchar32)(error_t **, void *, UChar32 *);
    UBool (*readline)(error_t **, void *, UString *);
    size_t (*readbytes)(void *, char *, size_t);                         /* Caller must append trailing \0 */
    int32_t (*readuchars)(error_t **, void *, UChar *, size_t);     /* Caller must append trailing \0 */
    int32_t (*readuchars32)(error_t **, void *, UChar32 *, size_t); /* Caller must append trailing \0 */
    UBool (*has_encoding)(void *);
    const char *(*get_encoding)(error_t **, void *);
    UBool (*set_encoding)(error_t **, void *, const char *);
    void (*rewind)(void *, int32_t); /* Caller must provide BOM length (as 2nd argument) */
} reader_imp_t;

# define STRING_REWIND(start, ptr, signature_length) \
    do {                                             \
        ptr = start + signature_length;              \
    } while (0);

# define STRING_READUCHAR32(error, ucnv, ptr, end, c)                     \
    do {                                                                  \
        UErrorCode status;                                                \
                                                                          \
        status = U_ZERO_ERROR;                                            \
        *c = ucnv_getNextUChar(ucnv, (const char **) &ptr, end, &status); \
        if (U_FAILURE(status)) {                                          \
            icu_error_set(error, FATAL, status, "ucnv_getNextUChar");     \
            return FALSE;                                                 \
        }                                                                 \
                                                                          \
        return TRUE;                                                      \
    } while (0);

# define STRING_READBYTES(ptr, end, buffer, max_len) \
    do {                                             \
        size_t n;                                    \
                                                     \
        if ((size_t) (end - ptr) > max_len) {        \
            n = max_len;                             \
        } else {                                     \
            n = end - ptr;                           \
        }                                            \
        memcpy(buffer, ptr, n);                      \
        ptr += n;                                    \
                                                     \
        return n;                                    \
    } while (0);

# define STRING_READUCHARS(error, ucnv, ptr, end, buffer, max_len)                               \
    do {                                                                                         \
        int count;                                                                               \
        UErrorCode status;                                                                       \
        UChar *dest;                                                                             \
        const UChar *uend;                                                                       \
                                                                                                 \
        status = U_ZERO_ERROR;                                                                   \
        dest = buffer;                                                                           \
        uend = buffer + max_len;                                                                 \
        ucnv_toUnicode(ucnv, &dest, uend, (const char **) &ptr, end, NULL, ptr >= end, &status); \
        if (U_FAILURE(status) && U_BUFFER_OVERFLOW_ERROR != status) {                            \
            icu_error_set(error, FATAL, status, "ucnv_toUnicode");                               \
            return -1;                                                                           \
        }                                                                                        \
        count = dest - buffer;                                                                   \
                                                                                                 \
        return count;                                                                            \
    } while (0);

# define STRING_READUCHARS32(error, ucnv, ptr, end, buffer, max_len)         \
    do {                                                                     \
        UChar32 c;                                                           \
        size_t i;                                                            \
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

# define STRING_SET_ENCODING(error, ucnv, encoding)           \
    do {                                                      \
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

# define STRING_EOF(ptr, end) \
    do {                      \
        return ptr >= end;    \
    } while (0);

# define STRING_SEEKABLE() \
    do {                   \
        return TRUE;       \
    } while (0);

#endif /* READER_H */
