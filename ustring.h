#ifndef UGREP_USTRING_H

# define UGREP_USTRING_H

typedef struct {
    UChar *ptr;
    size_t len;
    size_t allocated;
} UString;

void ustring_append_char(UString *, UChar);
void ustring_append_string(UString *, const UChar *);
void ustring_append_string_len(UString *, const UChar *, int32_t);
UChar *ustring_chomp(UString *);
void ustring_destroy(UString *);
UBool ustring_empty(const UString *);
UChar ustring_last_char(const UString *);
UString *ustring_new(void);
void ustring_sync(const UString *, UString *, double);
void ustring_truncate(UString *);

#endif /* UGREP_STRING_H */
