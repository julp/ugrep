#ifndef UGREP_USTRING_H

# define UGREP_USTRING_H

typedef struct {
    UChar *ptr;
    size_t len;
    size_t allocated;
} UString;

UString *ustring_adopt_string(UChar *);
UString *ustring_adopt_string_len(UChar *, int32_t);
void ustring_append_char(UString *, UChar);
void ustring_append_string(UString *, const UChar *);
void ustring_append_string_len(UString *, const UChar *, int32_t);
UChar *ustring_chomp(UString *);
void ustring_destroy(UString *);
UString *ustring_dup_string(const UChar *);
UString *ustring_dup_string_len(const UChar *, int32_t);
UBool ustring_empty(const UString *);
void ustring_insert_len(UString *, size_t, const UChar *, size_t);
UString *ustring_new(void);
void ustring_prepend_char(UString *, UChar);
void ustring_prepend_string(UString *, const UChar *);
void ustring_prepend_string_len(UString *, const UChar *, int32_t);
UString *ustring_sized_new(size_t);
void ustring_sync(const UString *, UString *, double);
UBool ustring_tolower(UString *, error_t **);
void ustring_truncate(UString *);

#endif /* UGREP_STRING_H */
