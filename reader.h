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
    const char *(*get_encoding)(void *); /* /!\ Don't call it without assuming a previous: TRUE == has_encoding /!\ */
    UBool (*set_encoding)(error_t **, void *, const char *);
    void (*rewind)(void *, int32_t); /* Caller must provide BOM length (as 2nd argument) */
} reader_imp_t;

#endif /* READER_H */
