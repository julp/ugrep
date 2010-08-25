#ifndef UGREP_USTRING_H

# define UGREP_USTRING_H

typedef struct {
    UChar *ptr;
    size_t len;
    size_t allocated;
} UString;

void ustring_append_char(UString *, UChar);
void ustring_append_string(UString *, const UChar *);
UChar *ustring_chomp(UString *);
void ustring_destroy(UString *);
UBool ustring_empty(const UString *);
void ustring_insert_len(UString *, size_t, const UChar *, size_t);
UString *ustring_new(void);
void ustring_prepend_char(UString *, UChar);
void ustring_prepend_string(UString *, const UChar *);
void ustring_sync(const UString *, UString *, double);
void ustring_truncate(UString *);

#endif /* UGREP_STRING_H */
