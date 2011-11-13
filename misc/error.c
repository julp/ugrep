#include "common.h"

#define ERROR_MAX_LEN 596

#ifdef DEBUG
const char *ubasename(const char *filename)
{
    const char *c;

    if (NULL == (c = strrchr(filename, DIRECTORY_SEPARATOR))) {
        return filename;
    } else {
        return c + 1;
    }
}
#endif /* DEBUG */

#ifdef _MSC_VER
# include <windows.h>

error_t *error_win32_vnew(int type, const char *format, va_list args) /* WARN_UNUSED_RESULT */
{
    LPWSTR *buf = NULL;
    error_t *error = NULL;
    int32_t length, buf_len;
    UChar buffer[ERROR_MAX_LEN + 1];

    FormatMessageW(
       FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
       NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR) &buf, 0, NULL
    );
    if (NULL != buf) {
        buf_len = u_strlen((UChar *) buf);
        error = mem_new(*error);
        length = u_vsnprintf(buffer, ERROR_MAX_LEN, format, args);
        error->type = type;
        error->message = mem_new_n(*error->message, length + buf_len + 1);
//         u_strcpy(error->message, buffer);
//         u_strcpy(error->message + length, buf, buf_len);
        u_memcpy(error->message, (UChar *) buffer, length);
        u_memcpy(error->message + length, (UChar *) buf, buf_len);
        error->message[length + buf_len] = 0;
        LocalFree(buf);
    }

    return error;
}

error_t *error_win32_new(int type, const char *format, ...) /* WARN_UNUSED_RESULT */
{
    error_t *error;
    va_list args;

    va_start(args, format);
    error = error_win32_vnew(type, format, args);
    va_end(args);

    return error;
}

# ifdef DEBUG
void _error_win32_set(error_t **error, int type, const char *format, ...)
# else
void error_win32_set(error_t **error, int type, const char *format, ...)
# endif /* DEBUG */
{
    va_list args;
    error_t *tmp;

    if (NULL != error) {
        va_start(args, format);
        tmp = error_win32_vnew(type, format, args);
        va_end(args);
        if (NULL == *error) {
            *error = tmp;
        } else {
            debug("overwrite attempt of a previous error: %S\nBy: %S", (*error)->message, tmp->message);
        }
    }
}
#endif /* _MSC_VER */

error_t *error_vnew(int type, const char *format, va_list args) /* WARN_UNUSED_RESULT */
{
    int32_t length;
    error_t *error;
    UChar buffer[ERROR_MAX_LEN + 1];

    error = mem_new(*error);
    length = u_vsnprintf(buffer, ERROR_MAX_LEN, format, args);
    error->type = type;
    error->message = mem_new_n(*error->message, length + 1);
    u_strcpy(error->message, buffer);

    return error;
}

error_t *error_new(int type, const char *format, ...) /* WARN_UNUSED_RESULT */
{
    error_t *error;
    va_list args;

    va_start(args, format);
    error = error_vnew(type, format, args);
    va_end(args);

    return error;
}

#ifdef DEBUG
void _error_set(error_t **error, int type, const char *format, ...)
#else
void error_set(error_t **error, int type, const char *format, ...)
#endif /* DEBUG */
{
    va_list args;
    error_t *tmp;

    if (NULL != error) {
        va_start(args, format);
        tmp = error_vnew(type, format, args);
        va_end(args);
        if (NULL == *error) {
            *error = tmp;
        } else {
            debug("overwrite attempt of a previous error: %S\nBy: %S", (*error)->message, tmp->message);
        }
    }
}

void error_destroy(error_t *error)
{
    if (NULL != error) {
        if (error->message) {
            free(error->message);
        }
        free(error);
        error = NULL;
    }
}

void error_propagate(error_t **dst, error_t *src)
{
    if (NULL == dst) {
        if (NULL != src) {
            error_destroy(src);
        }
    } else {
        if (NULL != *dst) {
            debug("overwrite attempt of a previous error: %S\nBy: %S", (*dst)->message, src->message);
        } else {
            *dst = src;
        }
    }
}
