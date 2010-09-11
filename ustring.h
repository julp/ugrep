#ifndef UGREP_USTRING_H

# define UGREP_USTRING_H

typedef struct {
    UChar *ptr;
    size_t len;
    size_t allocated;
} UString;

UString *ustring_adopt_string(UChar *) NONNULL();
UString *ustring_adopt_string_len(UChar *, size_t);
void ustring_append_char(UString *, UChar) NONNULL();
void ustring_append_string(UString *, const UChar *) NONNULL();
void ustring_append_string_len(UString *, const UChar *, int32_t) NONNULL();
void ustring_chomp(UString *) NONNULL();
void ustring_destroy(UString *) NONNULL();
void ustring_dump(UString *) NONNULL();
UString *ustring_dup_string(const UChar *) NONNULL();
UString *ustring_dup_string_len(const UChar *, size_t) NONNULL();
UBool ustring_empty(const UString *) NONNULL();
void ustring_insert_len(UString *, size_t, const UChar *, size_t) NONNULL();
UString *ustring_new(void) WARN_UNUSED_RESULT;
void ustring_prepend_char(UString *, UChar) NONNULL();
void ustring_prepend_string(UString *, const UChar *) NONNULL();
void ustring_prepend_string_len(UString *, const UChar *, int32_t) NONNULL();
UString *ustring_sized_new(size_t) WARN_UNUSED_RESULT;
void ustring_subreplace_len(UString *, const UChar *, size_t, size_t, size_t) NONNULL();
void ustring_sync(const UString *, UString *, double) NONNULL();
UBool ustring_tolower(UString *, error_t **) NONNULL(1);
void ustring_truncate(UString *) NONNULL();

#endif /* UGREP_STRING_H */
