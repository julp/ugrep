#ifndef READER_H

# define READER_H

enum {
    BIN_FILE_BIN,
    BIN_FILE_SKIP,
    BIN_FILE_TEXT
};

# define MIN_CONFIDENCE  39   // Minimum confidence for a match (in percents)
# define MAX_ENC_REL_LEN 4096 // Maximum relevant length for encoding analyse (in bytes)
# define MAX_BIN_REL_LEN 1024 // Maximum relevant length for binary analyse (in code points)

# ifdef DEBUG
#  define CHAR_BUFFER_SIZE  8
#  define UCHAR_BUFFER_SIZE 8
# else
#  define CHAR_BUFFER_SIZE  BUFSIZ
#  define UCHAR_BUFFER_SIZE BUFSIZ
# endif /* DEBUG */

// # if (CHAR_BUFFER_SIZE > MAX_ENC_REL_LEN) && (UCHAR_BUFFER_SIZE > (4 * MAX_BIN_REL_LEN))
// #  define NO_PHYSICAL_REWIND 1
// # endif

# ifdef DYNAMIC_READERS
#  define STRINGIFY(x) #x
#  if defined(_MSC_VER)
#   include <windows.h>
#   define DL_LOAD(name, major) LoadLibrary(name "lib" STRINGIFY(major) ".dll")
#   define DL_FETCH_SYMBOL      GetProcAddress
#   define DL_UNLOAD            FreeLibrary
#   define DL_HANDLE            HMODULE
#   define HAVE_DL_ERROR        0
#   define DL_ERROR
#  elif defined(HAVE_LIBDL)
#   include <dlfcn.h>
#   ifndef RTLD_LAZY
#    define RTLD_LAZY 1
#   endif /* !RTLD_LAZY */
#   ifndef RTLD_GLOBAL
#    define RTLD_GLOBAL 0
#   endif /* !RTLD_GLOBAL */
#   if defined(RTLD_GROUP) && defined(RTLD_WORLD) && defined(RTLD_PARENT)
#    define DL_LOAD(name, major) dlopen("lib" name ".so." STRINGIFY(major), RTLD_LAZY | RTLD_GLOBAL | RTLD_GROUP | RTLD_WORLD | RTLD_PARENT)
#   elif defined(RTLD_DEEPBIND)
#    define DL_LOAD(name, major) dlopen("lib" name ".so." STRINGIFY(major), RTLD_LAZY | RTLD_GLOBAL | RTLD_DEEPBIND)
#   else
#    define DL_LOAD(name, major) dlopen("lib" name ".so." STRINGIFY(major), RTLD_LAZY | RTLD_GLOBAL)
#   endif /* RTLD_GROUP && RTLD_WORLD && RTLD_PARENT */
#   define DL_UNLOAD             dlclose
#   define DL_FETCH_SYMBOL       dlsym
#   define DL_HANDLE             void *
#   define HAVE_DL_ERROR         1
#   define DL_ERROR              dlerror()
#  endif /* _MSC_VER */
# define DL_GET_SYM(handle, var, name) \
    do { \
        *(void **) &var = DL_FETCH_SYMBOL(handle, name); \
        if (!var) { \
            if (HAVE_DL_ERROR) { \
                debug("failed loading " name ": %s", DL_ERROR); \
            } else { \
                debug("failed loading " name); \
            } \
            DL_UNLOAD(handle); \
            return FALSE; \
        } \
    } while (0);
# endif /* DYNAMIC_READERS */

typedef struct {
    UBool internal;
    const char *name;
# ifdef DYNAMIC_READERS
    UBool (*available)(void);
# endif /* DYNAMIC_READERS */
    void *(*dopen)(error_t **, int, const char * const);
    void (*close)(void *);
    UBool (*eof)(void *);
    int32_t (*readBytes)(void *, error_t **, char *, size_t);
# ifndef NO_PHYSICAL_REWIND
    UBool (*rewindTo)(void *, error_t **, int32_t);
# endif /* !NO_PHYSICAL_REWIND */
} reader_imp_t;

typedef struct {
    const char *encoding;
    const char *sourcename;
    const char *default_encoding;
    const reader_imp_t *imp;
    const reader_imp_t *default_imp;
    void *priv_user;
    int binbehave;
    int32_t signature_length;
    size_t size;
    size_t lineno;
    UBool binary;

    int fd;   /* responsability of imp to close it if necessary */
    void *fp; /* responsability of imp to free and/or close it if necessary */
    UConverter *ucnv;
    struct {
        char buffer[CHAR_BUFFER_SIZE]; /* /!\ usage restricted to fill_buffer /!\ */
        char *ptr;                     /* /!\ usage restricted to fill_buffer /!\ */
        char *end;                     /* /!\ usage restricted to fill_buffer /!\ */
        const char *limit;             /* /!\ usage restricted to fill_buffer /!\ */
    } byte;
    struct {
        UChar buffer[UCHAR_BUFFER_SIZE];
        UChar *ptr;
        UChar *internalEnd; /* /!\ usage restricted to fill_buffer /!\ */
        UChar *externalEnd;
        const UChar *limit;
    } utf16;
} reader_t;

#define DEFAULT_READER_NAME "mmap"

void reader_close(reader_t *) NONNULL(1);
UBool reader_eof(reader_t *) NONNULL(1);
const reader_imp_t *reader_get_by_name(const char *);
void *reader_get_user_data(reader_t *) NONNULL(1);
void reader_init(reader_t *, const char *) NONNULL(1);
UBool reader_open(reader_t *, error_t **, const char *) NONNULL(1, 3);
UBool reader_open_stdin(reader_t *, error_t **) NONNULL(1);
UBool reader_open_string(reader_t *, error_t **, const char *) NONNULL(1, 3);
UBool reader_readline(reader_t *, error_t **, UString *) NONNULL(1, 3);
UChar32 reader_readuchar32(reader_t *, error_t **) NONNULL(1);
int32_t reader_readuchars(reader_t *, error_t **, UChar *, int32_t) NONNULL(1, 3);
int32_t reader_readuchars32(reader_t *, error_t **, UChar32 *, int32_t) NONNULL(1, 3);
void reader_set_binary_behavior(reader_t *, int) NONNULL(1);
void reader_set_default_encoding(reader_t *, const char *) NONNULL(1);
UBool reader_set_encoding(reader_t *, error_t **, const char *) NONNULL(1);
UBool reader_set_imp_by_name(reader_t *, const char *) NONNULL(1);
void reader_set_user_data(reader_t *, void *) NONNULL(1);

#endif /* !READER_H */
