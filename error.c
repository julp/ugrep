#include "ugrep.h"

#define ERROR_MAX_LEN 596

error_t *error_vnew(int type, const char *format, va_list args)
{
    int32_t length;
    error_t *error;
    UChar buffer[ERROR_MAX_LEN + 1];

    error = mem_new(*error);
    length = u_vsnprintf(buffer, ERROR_MAX_LEN, format, args);
    error->type = type;
    error->message = mem_new_n(*error->message, length);
    u_strcpy(error->message, buffer);

    return error;
}

error_t *error_new(int type, const char *format, ...)
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
            debug("overwrite attempt of a previous error: %s\nBy: %s", (*error)->message, tmp->message);
        }
    }
}

void error_destroy(error_t *error)
{
    if (error) {
        if (error->message) {
            free(error->message);
        }
        free(error);
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
            debug("overwrite attempt of a previous error: %s\nBy: %s", (*dst)->message, src->message);
        } else {
            *dst = src;
        }
    }
}
