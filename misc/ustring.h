#ifndef UGREP_USTRING_H

# define UGREP_USTRING_H

# include <unicode/ubrk.h>
# include "struct/darray.h"

typedef struct {
    UChar *ptr;
    size_t len;
    size_t allocated;
} UString;

typedef enum {
    UCASE_FAKE = -1, // quiet gcc
    UCASE_NONE,
    UCASE_FOLD,
    UCASE_LOWER,
    UCASE_UPPER,
    UCASE_TITLE,
    UCASE_COUNT
} UCaseType;

UString *ustring_adopt_string(UChar *) NONNULL();
UString *ustring_adopt_string_len(UChar *, size_t);
void ustring_append_char(UString *, UChar) NONNULL();
void ustring_append_char32(UString *, UChar32) NONNULL();
void ustring_append_string(UString *, const UChar *) NONNULL();
void ustring_append_string_len(UString *, const UChar *, int32_t) NONNULL();
UBool ustring_char32_len_cmp(const UString *, int, int32_t);
void ustring_chomp(UString *) NONNULL();
UString *ustring_convert_argv_from_local(const char *, error_t **, UBool);
int32_t ustring_delete_len(UString *, size_t, size_t) NONNULL(1);
void ustring_destroy(UString *) NONNULL();
void ustring_dump(UString *) NONNULL();
UString *ustring_dup(const UString *) WARN_UNUSED_RESULT NONNULL();
UString *ustring_dup_string(const UChar *) NONNULL();
UString *ustring_dup_string_len(const UChar *, size_t) NONNULL();
UBool ustring_empty(const UString *) NONNULL();
UBool ustring_endswith(UString *, UChar *, size_t) NONNULL();
UBool ustring_fullcase(UString *, UChar *, int32_t, UCaseType, error_t **) NONNULL(1);
void ustring_index(UString *, UBreakIterator *, DArray *) NONNULL();
int32_t ustring_insert_len(UString *, size_t, const UChar *, size_t) NONNULL(1);
void ustring_ltrim(UString *) NONNULL();
UString *ustring_new(void) WARN_UNUSED_RESULT;
UBool ustring_normalize(UString *, UNormalizationMode) NONNULL(1);
UChar *ustring_orphan(UString *) NONNULL();
void ustring_prepend_char(UString *, UChar) NONNULL();
void ustring_prepend_string(UString *, const UChar *) NONNULL();
void ustring_prepend_string_len(UString *, const UChar *, int32_t) NONNULL();
void ustring_rtrim(UString *) NONNULL();
UString *ustring_sized_new(size_t) WARN_UNUSED_RESULT;
void ustring_sprintf(UString *, const char *, ...) PRINTF(2, 3) NONNULL(1, 2);
UBool ustring_startswith(UString *, UChar *, size_t) NONNULL();
int32_t ustring_subreplace_len(UString *, const UChar *, size_t, size_t, size_t) NONNULL(1);
void ustring_sync(const UString *, UString *, double) NONNULL();
void *ustring_to_collation_key(const void *, const void *) NONNULL();
UBool ustring_transliterate(UString *, UTransliterator *, error_t **) NONNULL(1, 2);
void ustring_trim(UString *) NONNULL();
void ustring_truncate(UString *) NONNULL();
void ustring_unescape(UString *) NONNULL();

#endif /* UGREP_STRING_H */
