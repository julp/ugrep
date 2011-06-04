#ifndef READER_H

# define READER_H

typedef struct {
    const char *name;
    void *(*open)(error_t **, const char *, int);
    void (*close)(void *);
    UBool (*eof)(void *);
    UBool (*seekable)(void *);
    UBool (*readline)(error_t **error, void *, UString *);
    size_t (*readbytes)(void *, char *, size_t);
    int32_t (*readuchars)(error_t **error, void *, UChar32 *, size_t);
    UBool (*set_encoding)(error_t **, void *, const char *);
    void (*rewind)(void *, int32_t);
} reader_t;

#endif /* READER_H */
